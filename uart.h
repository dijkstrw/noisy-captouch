#ifndef _UART_H
#define _UART_H

volatile char UARTRxData[20];
volatile char UARTRxFlag;

void uart_configure(void);
void uart_putc(uint8_t c);
void uart_send_array(uint8_t *s, uint8_t len);
void uart_send_string(uint8_t *s);

#endif
