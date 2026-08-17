# EngrZoroz ESP32-S3 CAM + 3.5" ILI9486

Custom XiaoZhi board definition for:

- ESP32-S3-WROOM-1 N16R8 camera board with OV3660-style DVP pinout
- INMP441 I2S microphone
- MAX98357A I2S amplifier
- 4-ohm speaker
- 3.5-inch Arduino/UNO-style 8-bit parallel TFT shield
- ILI9486, 480x320, landscape

The LCD uses the ESP-IDF I80 driver. Touch and microSD are intentionally disabled
in this first hardware-stable revision because the full XiaoZhi + camera + audio
build already consumes nearly every safe GPIO.

Important: GPIO19 and GPIO20 are used by this build, so the ESP32-S3 native USB
JTAG/OTG function on those GPIOs is not available while the circuit is connected.
Program through the board's CH343/USB-UART Type-C port.
