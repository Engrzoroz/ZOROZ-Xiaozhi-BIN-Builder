#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "esp32_camera.h"

#include <esp_check.h>
#include <esp_log.h>
#include <esp_lcd_io_i80.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_commands.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdlib>

#define TAG "EngrZorozS3CamILI9486"

namespace {

struct Ili9486Panel {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    bool mirror_x;
    bool mirror_y;
    bool swap_xy;
};

static inline Ili9486Panel* AsIli9486(esp_lcd_panel_t* panel) {
    return reinterpret_cast<Ili9486Panel*>(panel);
}

static esp_err_t TxParam(esp_lcd_panel_io_handle_t io, int cmd,
                         const uint8_t* data, size_t len) {
    return esp_lcd_panel_io_tx_param(io, cmd, data, len);
}

static esp_err_t UpdateMadctl(Ili9486Panel* lcd) {
    uint8_t madctl = ILI9486_BASE_MADCTL;
    if (lcd->mirror_x) {
        madctl ^= LCD_CMD_MX_BIT;
    }
    if (lcd->mirror_y) {
        madctl ^= LCD_CMD_MY_BIT;
    }
    if (lcd->swap_xy) {
        madctl ^= LCD_CMD_MV_BIT;
    }
    return TxParam(lcd->io, LCD_CMD_MADCTL, &madctl, 1);
}

static esp_err_t Ili9486Del(esp_lcd_panel_t* panel) {
    free(AsIli9486(panel));
    return ESP_OK;
}

static esp_err_t Ili9486Reset(esp_lcd_panel_t* panel) {
    auto* lcd = AsIli9486(panel);
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, LCD_CMD_SWRESET, nullptr, 0),
                        TAG, "ILI9486 SWRESET failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t Ili9486Init(esp_lcd_panel_t* panel) {
    auto* lcd = AsIli9486(panel);

    static const uint8_t f2[] = {0x1C,0xA3,0x32,0x02,0xB2,0x12,0xFF,0x12,0x00};
    static const uint8_t f1[] = {0x36,0xA4};
    static const uint8_t f8[] = {0x21,0x04};
    static const uint8_t f9[] = {0x00,0x08};
    static const uint8_t c0[] = {0x0D,0x0D};
    static const uint8_t c1[] = {0x43,0x00};
    static const uint8_t c2[] = {0x00};
    static const uint8_t c5[] = {0x00,0x48};
    static const uint8_t b6[] = {0x00,0x22,0x3B};
    static const uint8_t gp[] = {
        0x0F,0x24,0x1C,0x0A,0x0F,0x08,0x43,0x88,
        0x32,0x0F,0x10,0x06,0x0F,0x07,0x00
    };
    static const uint8_t gn[] = {
        0x0F,0x38,0x30,0x09,0x0F,0x0F,0x4E,0x77,
        0x3C,0x07,0x10,0x05,0x23,0x1B,0x00
    };
    static const uint8_t colmod[] = {0x55};

    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xF2, f2, sizeof(f2)), TAG, "F2");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xF1, f1, sizeof(f1)), TAG, "F1");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xF8, f8, sizeof(f8)), TAG, "F8");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xF9, f9, sizeof(f9)), TAG, "F9");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xC0, c0, sizeof(c0)), TAG, "C0");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xC1, c1, sizeof(c1)), TAG, "C1");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xC2, c2, sizeof(c2)), TAG, "C2");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xC5, c5, sizeof(c5)), TAG, "C5");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xB6, b6, sizeof(b6)), TAG, "B6");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xE0, gp, sizeof(gp)), TAG, "E0");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, 0xE1, gn, sizeof(gn)), TAG, "E1");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, LCD_CMD_COLMOD, colmod, sizeof(colmod)),
                        TAG, "COLMOD");
    ESP_RETURN_ON_ERROR(UpdateMadctl(lcd), TAG, "MADCTL");

    ESP_RETURN_ON_ERROR(TxParam(lcd->io, LCD_CMD_SLPOUT, nullptr, 0),
                        TAG, "SLPOUT");
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, LCD_CMD_DISPON, nullptr, 0),
                        TAG, "DISPON");
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

static esp_err_t Ili9486DrawBitmap(esp_lcd_panel_t* panel,
                                   int x_start, int y_start,
                                   int x_end, int y_end,
                                   const void* color_data) {
    auto* lcd = AsIli9486(panel);

    x_start += lcd->x_gap;
    x_end += lcd->x_gap;
    y_start += lcd->y_gap;
    y_end += lcd->y_gap;

    uint8_t col[] = {
        static_cast<uint8_t>((x_start >> 8) & 0xFF),
        static_cast<uint8_t>(x_start & 0xFF),
        static_cast<uint8_t>(((x_end - 1) >> 8) & 0xFF),
        static_cast<uint8_t>((x_end - 1) & 0xFF)
    };
    uint8_t row[] = {
        static_cast<uint8_t>((y_start >> 8) & 0xFF),
        static_cast<uint8_t>(y_start & 0xFF),
        static_cast<uint8_t>(((y_end - 1) >> 8) & 0xFF),
        static_cast<uint8_t>((y_end - 1) & 0xFF)
    };

    ESP_RETURN_ON_ERROR(TxParam(lcd->io, LCD_CMD_CASET, col, sizeof(col)),
                        TAG, "CASET");
    ESP_RETURN_ON_ERROR(TxParam(lcd->io, LCD_CMD_RASET, row, sizeof(row)),
                        TAG, "RASET");

    size_t len = static_cast<size_t>(x_end - x_start) *
                 static_cast<size_t>(y_end - y_start) *
                 lcd->fb_bits_per_pixel / 8;

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_color(lcd->io, LCD_CMD_RAMWR, color_data, len),
        TAG, "RAMWR");
    return ESP_OK;
}

static esp_err_t Ili9486InvertColor(esp_lcd_panel_t* panel, bool invert) {
    auto* lcd = AsIli9486(panel);
    return TxParam(lcd->io, invert ? LCD_CMD_INVON : LCD_CMD_INVOFF, nullptr, 0);
}

static esp_err_t Ili9486Mirror(esp_lcd_panel_t* panel, bool mirror_x, bool mirror_y) {
    auto* lcd = AsIli9486(panel);
    lcd->mirror_x = mirror_x;
    lcd->mirror_y = mirror_y;
    return UpdateMadctl(lcd);
}

static esp_err_t Ili9486SwapXY(esp_lcd_panel_t* panel, bool swap_axes) {
    auto* lcd = AsIli9486(panel);
    lcd->swap_xy = swap_axes;
    return UpdateMadctl(lcd);
}

static esp_err_t Ili9486SetGap(esp_lcd_panel_t* panel, int x_gap, int y_gap) {
    auto* lcd = AsIli9486(panel);
    lcd->x_gap = x_gap;
    lcd->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t Ili9486DispOnOff(esp_lcd_panel_t* panel, bool on) {
    auto* lcd = AsIli9486(panel);
    return TxParam(lcd->io, on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, nullptr, 0);
}

static esp_err_t Ili9486Sleep(esp_lcd_panel_t* panel, bool sleep) {
    auto* lcd = AsIli9486(panel);
    esp_err_t err = TxParam(lcd->io, sleep ? LCD_CMD_SLPIN : LCD_CMD_SLPOUT,
                            nullptr, 0);
    vTaskDelay(pdMS_TO_TICKS(sleep ? 5 : 120));
    return err;
}

static esp_err_t Ili9486SetBrightness(esp_lcd_panel_t*, int) {
    // Backlight on this Arduino shield is powered directly; no separate BL pin.
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t NewIli9486Panel(esp_lcd_panel_io_handle_t io,
                                 esp_lcd_panel_handle_t* ret_panel) {
    ESP_RETURN_ON_FALSE(io && ret_panel, ESP_ERR_INVALID_ARG, TAG,
                        "invalid ILI9486 arguments");

    auto* lcd = static_cast<Ili9486Panel*>(calloc(1, sizeof(Ili9486Panel)));
    ESP_RETURN_ON_FALSE(lcd, ESP_ERR_NO_MEM, TAG, "no memory for ILI9486");

    lcd->io = io;
    lcd->fb_bits_per_pixel = 16;
    lcd->base.reset = Ili9486Reset;
    lcd->base.init = Ili9486Init;
    lcd->base.del = Ili9486Del;
    lcd->base.draw_bitmap = Ili9486DrawBitmap;
    lcd->base.draw_bitmap_2d = nullptr;
    lcd->base.mirror = Ili9486Mirror;
    lcd->base.swap_xy = Ili9486SwapXY;
    lcd->base.set_gap = Ili9486SetGap;
    lcd->base.invert_color = Ili9486InvertColor;
    lcd->base.disp_on_off = Ili9486DispOnOff;
    lcd->base.disp_sleep = Ili9486Sleep;
    lcd->base.set_brightness = Ili9486SetBrightness;
    lcd->base.user_data = nullptr;

    *ret_panel = &lcd->base;
    return ESP_OK;
}

}  // namespace

class EngrZorozS3CamIli9486Board : public WifiBoard {
private:
    Button boot_button_;
    LcdDisplay* display_ = nullptr;
    Esp32Camera* camera_ = nullptr;
    esp_lcd_i80_bus_handle_t i80_bus_ = nullptr;

    void InitializeLcdDisplay() {
        ESP_LOGI(TAG, "Initializing 8-bit I80 bus for 3.5in ILI9486");

        esp_lcd_i80_bus_config_t bus_config = {};
        bus_config.clk_src = LCD_CLK_SRC_DEFAULT;
        bus_config.dc_gpio_num = DISPLAY_DC_PIN;
        bus_config.wr_gpio_num = DISPLAY_WR_PIN;
        bus_config.data_gpio_nums[0] = DISPLAY_I80_D0;
        bus_config.data_gpio_nums[1] = DISPLAY_I80_D1;
        bus_config.data_gpio_nums[2] = DISPLAY_I80_D2;
        bus_config.data_gpio_nums[3] = DISPLAY_I80_D3;
        bus_config.data_gpio_nums[4] = DISPLAY_I80_D4;
        bus_config.data_gpio_nums[5] = DISPLAY_I80_D5;
        bus_config.data_gpio_nums[6] = DISPLAY_I80_D6;
        bus_config.data_gpio_nums[7] = DISPLAY_I80_D7;
        bus_config.bus_width = 8;
        bus_config.max_transfer_bytes = DISPLAY_WIDTH * 40 * sizeof(uint16_t);
        bus_config.dma_burst_size = 64;
        ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus_));

        esp_lcd_panel_io_i80_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.pclk_hz = DISPLAY_I80_PCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        io_config.dc_levels.dc_idle_level = 0;
        io_config.dc_levels.dc_cmd_level = 0;
        io_config.dc_levels.dc_dummy_level = 0;
        io_config.dc_levels.dc_data_level = 1;
        io_config.flags.cs_active_high = 0;
        io_config.flags.reverse_color_bits = 0;
        io_config.flags.swap_color_bytes = 0;
        io_config.flags.pclk_active_neg = 0;
        io_config.flags.pclk_idle_low = 0;

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus_, &io_config, &panel_io));

        esp_lcd_panel_handle_t panel = nullptr;
        ESP_ERROR_CHECK(NewIli9486Panel(panel_io, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

        // Landscape 480x320 is already encoded as the panel's base MADCTL.
        // Keep LVGL rotation flags false so it uses the entire 480x320 canvas.
        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     false, false, false);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeCamera() {
        camera_config_t config = {};
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = CAMERA_PIN_SIOD;
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.sccb_i2c_port = 0;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32Camera(config);
    }

public:
    EngrZorozS3CamIli9486Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeCamera();
    }

    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        return nullptr;
    }

    Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(EngrZorozS3CamIli9486Board);
