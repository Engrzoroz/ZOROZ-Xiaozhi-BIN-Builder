#ifndef _ENGRZOROZ_S3CAM_ILI9486_CONFIG_H_
#define _ENGRZOROZ_S3CAM_ILI9486_CONFIG_H_

#include <driver/gpio.h>

// -----------------------------------------------------------------------------
// Audio: shared-clock full-duplex I2S
// INMP441 + MAX98357A share BCLK and WS/LRCLK.
// XiaoZhi's current NoAudioCodecDuplex initializes RX and TX from the same
// output clock, so both rates are deliberately 24 kHz.
// -----------------------------------------------------------------------------
#define AUDIO_INPUT_SAMPLE_RATE   24000
#define AUDIO_OUTPUT_SAMPLE_RATE  24000

#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_1
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_2
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_42
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_19

// BOOT button already exists on this ESP32-S3-CAM board.
#define BOOT_BUTTON_GPIO GPIO_NUM_0

// -----------------------------------------------------------------------------
// OV3660 / DVP camera pins: matching the ESP32-S3-CAM board used in the
// referenced XiaoZhi/SKR-style build.
// -----------------------------------------------------------------------------
#define CAMERA_PIN_D0    GPIO_NUM_11
#define CAMERA_PIN_D1    GPIO_NUM_9
#define CAMERA_PIN_D2    GPIO_NUM_8
#define CAMERA_PIN_D3    GPIO_NUM_10
#define CAMERA_PIN_D4    GPIO_NUM_12
#define CAMERA_PIN_D5    GPIO_NUM_18
#define CAMERA_PIN_D6    GPIO_NUM_17
#define CAMERA_PIN_D7    GPIO_NUM_16
#define CAMERA_PIN_XCLK  GPIO_NUM_15
#define CAMERA_PIN_PCLK  GPIO_NUM_13
#define CAMERA_PIN_VSYNC GPIO_NUM_6
#define CAMERA_PIN_HREF  GPIO_NUM_7
#define CAMERA_PIN_SIOC  GPIO_NUM_5
#define CAMERA_PIN_SIOD  GPIO_NUM_4
#define CAMERA_PIN_PWDN  GPIO_NUM_NC
#define CAMERA_PIN_RESET GPIO_NUM_NC
#define XCLK_FREQ_HZ     20000000

// -----------------------------------------------------------------------------
// 3.5" Arduino-style 8-bit parallel ILI9486 shield, 480x320 landscape.
// LCD_RD is tied HIGH to 3V3 and LCD_RST is tied HIGH to 3V3.
// SD and resistive touch are intentionally not connected in v1.
// -----------------------------------------------------------------------------
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  320
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_I80_D0 GPIO_NUM_38
#define DISPLAY_I80_D1 GPIO_NUM_39
#define DISPLAY_I80_D2 GPIO_NUM_40
#define DISPLAY_I80_D3 GPIO_NUM_41
#define DISPLAY_I80_D4 GPIO_NUM_3
#define DISPLAY_I80_D5 GPIO_NUM_46
#define DISPLAY_I80_D6 GPIO_NUM_47
#define DISPLAY_I80_D7 GPIO_NUM_48

#define DISPLAY_CS_PIN GPIO_NUM_14
#define DISPLAY_DC_PIN GPIO_NUM_21
#define DISPLAY_WR_PIN GPIO_NUM_20

// No GPIO is consumed for these two lines.
#define DISPLAY_RD_PIN  GPIO_NUM_NC
#define DISPLAY_RST_PIN GPIO_NUM_NC

// Conservative I80 clock for long Dupont wires / Arduino shield routing.
#define DISPLAY_I80_PCLK_HZ 8000000

// The tested landscape orientation for this shield is MADCTL 0x68.
// The custom panel driver preserves this as the base orientation.
#define ILI9486_BASE_MADCTL 0x68

#endif
