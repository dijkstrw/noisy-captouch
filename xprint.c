#include <stdint.h>
#include "uart.h"
#include "xprint.h"

static uint8_t
itoa(int16_t value, uint16_t radix, uint16_t uppercase, uint16_t unsig,
          char *buffer, uint16_t zero_pad)
{
    char *pbuffer = buffer;
    int16_t negative = 0;
    uint16_t i, len;

    /* No support for unusual radixes. */
    if (radix > 16)
        return 0;

    if (value < 0 && !unsig) {
        negative = 1;
        value = -value;
    }

    /* This builds the string back to front ... */
    do {
        int digit = value % radix;
        *(pbuffer++) = (digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10);
        value /= radix;
    } while (value > 0);

    for (i = (pbuffer - buffer); i < zero_pad; i++)
        *(pbuffer++) = '0';

    if (negative)
        *(pbuffer++) = '-';

    *(pbuffer) = '\0';

    /* ... now we reverse it (could do it recursively but will
     * conserve the stack space) */
    len = (pbuffer - buffer);
    for (i = 0; i < len / 2; i++) {
        char j = buffer[i];
        buffer[i] = buffer[len-i-1];
        buffer[len-i-1] = j;
    }

    return len;
}

static void
xvprintf(const char *fmt, va_list va)
{
    char bf[12];
    char ch;

    while ((ch = *(fmt++))) {
        if (ch == '\n') {
            uart_send_char('\n');
            uart_send_char('\r');
        } else if (ch != '%') {
            uart_send_char(ch);
        } else {
            char zero_pad = 0;
            char *ptr;
            uint8_t len;

            ch = *(fmt++);

            /* Zero padding requested */
            if (ch == '0') {
                ch = *(fmt++);
                if (ch == '\0')
                    return;
                if (ch >= '0' && ch <= '9')
                    zero_pad = ch - '0';
                ch = *(fmt++);
            }

            switch (ch) {
            case 0:
                return;

            case 'u':
            case 'd':
                len = itoa(va_arg(va, uint16_t), 10, 0, (ch == 'u'), bf, zero_pad);
                uart_send_array((uint8_t *)bf, len);
                break;

            case 'x':
            case 'X':
                len = itoa(va_arg(va, uint16_t), 16, (ch == 'X'), 1, bf, zero_pad);
                uart_send_array((uint8_t *)bf, len);
                break;

            case 'c' :
                uart_send_char((char)(va_arg(va, int)));
                break;

            case 's' :
                ptr = va_arg(va, char*);
                uart_send_string((uint8_t *)ptr);
                break;

            default:
                uart_send_char(ch);
                break;
            }
        }
    }
}


void
xprintf(const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    xvprintf(fmt, va);
    va_end(va);
}
