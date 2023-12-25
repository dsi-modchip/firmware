
#ifndef PINOUT_WS_H_
#define PINOUT_WS_H_

#define PINOUT_UART_TX    0
#define PINOUT_UART_RX    1
#define PINOUT_UART_DEVNO 0
#define PINOUT_UART_DEV   uart0

#define PINOUT_SPI_CIPO   29
#define PINOUT_SPI_COPI   28
#define PINOUT_SPI_SCLK   27
#define PINOUT_SPI_nCS_IN 26 /* incoming nCS line */
// use pio1, pio0 is for glitching
#define PINOUT_SPI_PIODEVNO 1
#define PINOUT_SPI_PIODEV pio1_hw

#define PINOUT_nRESET 15
#define PINOUT_GLITCH_OUT 14

#define PINOUT_LED_WS2812 16
#define PINOUT_LED_WS2812_PIODEVNO 1
#define PINOUT_LED_WS2812_PIODEV   pio1_hw

#endif

