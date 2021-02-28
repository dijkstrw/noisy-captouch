/*
 * MSP430 minimal xprintf
 *
 * Based on mini-printf:
 * Copyright (c) 2013,2014 Michal Ludvig <michal@logix.cz>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the auhor nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
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
            uart_putc('\n');
            uart_putc('\r');
        } else if (ch != '%') {
            uart_putc(ch);
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
                len = itoa(va_arg(va, unsigned int), 10, 0, (ch == 'u'), bf, zero_pad);
                uart_send_array((uint8_t *)bf, len);
                break;

            case 'x':
            case 'X':
                len = itoa(va_arg(va, unsigned int), 16, (ch == 'X'), 1, bf, zero_pad);
                uart_send_array((uint8_t *)bf, len);
                break;

            case 'c' :
                uart_putc((char)(va_arg(va, int)));
                break;

            case 's' :
                ptr = va_arg(va, char*);
                uart_send_string((uint8_t *)ptr);
                break;

            default:
                uart_putc(ch);
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
