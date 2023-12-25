
#ifndef PINOUT_REV1_H_
#define PINOUT_REV1_H_

#define PINOUT_SDIO_CLK  0
#define PINOUT_SDIO_DAT0 1
#define PINOUT_SDIO_CMD  2

#define PINOUT_WIFI_nRST 3

#if HAXMODE_DEV_FEATURES
#define PINOUT_DEV_I2C_SDA 4
#define PINOUT_DEV_I2C_SCL 5
#define PINOUT_DEV_PWSW    6
#define PINOUT_DEV_POWERCUT 7
#define PINOUT_DEV_GPIO330  8
#define PINOUT_DEV_I2C_DEVNO 0
#define PINOUT_DEV_I2C_DEV i2c0
#define PINOUT_DEV_I2C_ADDR 0x42
#endif

#define PINOUT_RTC_FOUT  9
#define PINOUT_nSEL_ATH 10

#define PINOUT_SPI_nCS_IN 11 /* incoming nCS line */
#define PINOUT_SPI_CIPO   12
#define PINOUT_SPI_nCS2   13 /* to wifiboard flash (if attached) */
#define PINOUT_SPI_SCLK   14
#define PINOUT_SPI_COPI   15
#define PINOUT_SPI_DEVNO  1
#define PINOUT_SPI_DEV    spi1
// use pio1, pio0 is for glitching
#define PINOUT_SPI_PIODEVNO 1
#define PINOUT_SPI_PIODEV pio1_hw

#define PINOUT_UART_TX    16
#define PINOUT_UART_RX    17
#define PINOUT_UART_DEVNO 0
#define PINOUT_UART_DEV   uart0

#define PINOUT_nRESET 18
#define PINOUT_GLITCH_OUT 20

#define PINOUT_SPI_nCS3 22 /* to custom flash */

#define PINOUT_USB_DET 23
#define PINOUT_MBRD_DET 24

#define PINOUT_LED_B2 25
#define PINOUT_LED_P2 26
#define PINOUT_LED_W  27
#define PINOUT_LED_P1 28
#define PINOUT_LED_B1 29

#endif

