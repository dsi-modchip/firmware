
#ifndef PINOUT_ITSYBITSY_H_
#define PINOUT_ITSYBITSY_H_

#define PINOUT_UART_TX    0
#define PINOUT_UART_RX    1
#define PINOUT_UART_DEVNO 0
#define PINOUT_UART_DEV   uart0

#define PINOUT_nRESET 24
#define PINOUT_GLITCH_OUT 25

#define PINOUT_SPI_SCLK   18
#define PINOUT_SPI_COPI   19
#define PINOUT_SPI_CIPO   20
#define PINOUT_SPI_nCS_IN 12 /* incoming nCS line */
// use pio1, pio0 is for glitching
#define PINOUT_SPI_PIODEVNO 1
#define PINOUT_SPI_PIODEV pio1_hw

#define PINOUT_LED_WS2812 17
#define PINOUT_LED_WS2812_PIODEVNO 1
#define PINOUT_LED_WS2812_PIODEV   pio1_hw

#define PINOUT_LED_WS2812_POWER 16

#endif

