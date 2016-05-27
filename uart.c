/*
 * Simple msp430 uart out implementation
 *
 * This encapsulates two approaches:
 * - use the USCI as UART (MSP430G2553 et al)
 * - use TIMER0_A0 to bitbang otherwise (MSP430G2452 et al)
 */
#include <stdint.h>
#include <msp430.h>
#include "uart.h"

#define RXD BIT1
#define TXD BIT2

#ifdef __MSP430_HAS_USCI__

#define WAIT_TX_READY()     while (!(IFG2 & UCA0TXIFG))

void
uart_configure(void)
{
    P1SEL = TXD + RXD;
    P1SEL2 = TXD + RXD;
    UCA0CTL1 |= UCSSEL_2;  /* Use SMCLK */
    UCA0BR0 = 104;         /* Set baud rate to 9600 with 1MHz clock (Data Sheet 15.3.13) */
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS0;     /* Modulation UCBRSx = 1 */
    UCA0CTL1 &= ~UCSWRST;  /* Initialize USCI state machine */
}

void
uart_putc(uint8_t c)
{
    WAIT_TX_READY();
    UCA0TXBUF = c;
}
#else

#define FCPU 1000000
#define BAUDRATE 9600

#define BIT_TIME        (FCPU / BAUDRATE)
#define HALF_BIT_TIME   (BIT_TIME / 2)

static uint8_t bitcount;
static uint16_t tx;

void
uart_configure(void)
{
    P1SEL |= TXD + RXD;
    P1DIR |= TXD + RXD;
}

void
uart_putc(uint8_t c)
{
    tx = c;

    CCTL0 = OUT;             /* TXD Idle as Mark */
    TACTL = TASSEL_2 + MC_2; /* SMCLK, continuous mode */

    bitcount = 8 + 1 + 1;    /* Load Bit counter: 8 + start + stop */
    CCR0 = TAR;              /* Initialize compare register */

    CCR0 += BIT_TIME;        /* Set time till first bit */
    tx |= 0x100;             /* Add stop bit to transmit byte */
    tx <<= 1;                /* Add start bit */

    CCTL0 = CCIS1 + OUTMOD0 + CCIE; /* Set signal, initial value, enable int */

    while ( CCTL0 & CCIE );  /* Wait for previous TX completion */
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void
Timer_A(void)
{
    CCR0 += BIT_TIME;
    if (bitcount == 0) {
        TACTL = TASSEL_2;    /* SMCLK, timer off */
        CCTL0 &= ~ CCIE ;
    } else {
        if (tx & 1)
            CCTL0 &= ~ OUTMOD2;
        else
            CCTL0 |= OUTMOD2;
        tx >>= 1;
        bitcount--;
    }
}

#endif

void
uart_send_array(uint8_t *s, uint8_t len)
{
    while (len--)
        uart_putc(*s++);
}

void
uart_send_string(uint8_t *s){
    while (*s)
        uart_putc(*s++);
}
