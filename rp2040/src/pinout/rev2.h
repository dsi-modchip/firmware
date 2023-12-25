
#ifndef PINOUT_REV2_H_
#define PINOUT_REV2_H_

#define HAXBOARD_MAX_SPEED_MHZ 200

#define PINOUT_nRESET 0

#define PINOUT_SPI_nCS3   1
#define PINOUT_SPI_COPI   3
#define PINOUT_SPI_CIPO   4
#define PINOUT_SPI_nCS2   5
#define PINOUT_SPI_SCLK   6
#define PINOUT_SPI_nCS_IN 7
#define PINOUT_SPI_DEVNO  0
#define PINOUT_SPI_DEV    spi0
// use pio1, pio0 is for glitching
#define PINOUT_SPI_PIODEVNO 1
#define PINOUT_SPI_PIODEV   pio1_hw

#if HAXMODE_DEV_FEATURES
#define PINOUT_GPIO330  10
#define PINOUT_POWERCUT 12
#else
#define PINOUT_R7HINGE  12
#endif

#define PINOUT_RTC_FOUT  11
#define PINOUT_SDIO_CMD  13
#define PINOUT_SDIO_DAT0 14
#define PINOUT_SDIO_CLK  15

#if HAXMODE_DEV_FEATURES
#define PINOUT_PWSW 18
#endif

#define PINOUT_LED_WS2812 20
#define PINOUT_LED_WS2812_PIODEVNO 1
#define PINOUT_LED_WS2812_PIODEV   pio1_hw

#define PINOUT_MBRD_DET   21
#define PINOUT_GLITCH_OUT 24

#if HAXMODE_DEV_FEATURES
#define PINOUT_DEV_I2C_SDA   26
#define PINOUT_DEV_I2C_SCL   27
#define PINOUT_DEV_I2C_DEVNO 1
#define PINOUT_DEV_I2C_DEV   i2c1
#define PINOUT_DEV_I2C_ADDR  0x42
#endif

#define PINOUT_UART_TX    28
#define PINOUT_UART_RX    29
#define PINOUT_UART_DEVNO 0
#define PINOUT_UART_DEV   uart0

#endif
