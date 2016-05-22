#ifndef _UART_H
#define _UART_H

void uart_configure(void);
void uart_putc(uint8_t c);
void uart_send_array(uint8_t *s, uint8_t len);
void uart_send_string(uint8_t *s);

#endif
