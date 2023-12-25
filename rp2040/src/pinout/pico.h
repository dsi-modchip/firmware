
#ifndef PINOUT_PICO_H_
#define PINOUT_PICO_H_


// necessary things: pin 35..27 == GPIO 21..28

#define PINOUT_SPI_SCLK   28
#define PINOUT_SPI_COPI   27
#define PINOUT_SPI_nCS_IN /*26*/7 /* incoming nCS line */
// use pio1, pio0 is for glitching
#define PINOUT_SPI_PIODEVNO 1
#define PINOUT_SPI_PIODEV pio1_hw

#define PINOUT_nRESET 22
#define PINOUT_GLITCH_OUT 21

// other things, for dev mode only

#define PINOUT_UART_TX    0
/*#define PINOUT_UART_RX    1*/
#define PINOUT_UART_DEVNO 0
#define PINOUT_UART_DEV   uart0

#define PINOUT_USB_DET 24

#if HAXMODE_DEV_FEATURES
#define PINOUT_DEV_I2C_SDA 4
#define PINOUT_DEV_I2C_SCL 5
#define PINOUT_DEV_I2C_DEVNO 0
#define PINOUT_DEV_I2C_DEV i2c0
#define PINOUT_DEV_I2C_ADDR 0x42

#define PINOUT_DEV_PWSW     6
#define PINOUT_DEV_POWERCUT 7
#define PINOUT_DEV_GPIO330  8
#define PINOUT_DEV_R7HINGE  9

#define PINOUT_SPI_CIPO 12
#define PINOUT_SPI_nCS2 13 /* to wifiboard flash (if attached) */
#define PINOUT_SPI_nCS3 /*25*//*22*/1 /* to custom flash */
#endif

#endif

