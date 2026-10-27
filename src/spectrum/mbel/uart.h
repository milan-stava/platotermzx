#ifndef MBEL_UART_H
#define MBEL_UART_H

void wifi_uart_init(void);
/* -1: empty, -2: FIFO overflow; otherwise an unsigned byte. */
int wifi_uart_read(void);
int wifi_uart_write(unsigned char value) __z88dk_fastcall;
unsigned int wifi_ticks(void);

#endif
