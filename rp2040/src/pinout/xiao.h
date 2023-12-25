
#ifndef PINOUT_XIAO_H_
#define PINOUT_XIAO_H_

#define PINOUT_SPI_nCS_IN 3 /* incoming nCS line */
#define PINOUT_SPI_COPI   4
#define PINOUT_SPI_SCLK   2
// use pio1, pio0 is for glitching
#define PINOUT_SPI_PIODEVNO 1
#define PINOUT_SPI_PIODEV pio1_hw

#define PINOUT_UART_TX    0
#define PINOUT_UART_RX    1
#define PINOUT_UART_DEVNO 0
#define PINOUT_UART_DEV   uart0


#define PINOUT_nRESET 6
#define PINOUT_GLITCH_OUT 7

#define PINOUT_LED_WS2812 12
#define PINOUT_LED_WS2812_PIODEVNO 1
#define PINOUT_LED_WS2812_PIODEV   pio1_hw

#endif

