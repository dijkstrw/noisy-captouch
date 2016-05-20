/*
 * Simple msp430 uart out implementation
 *
 * This encapsulates two approaches:
 * - use the USCI as UART for the MSP430G2553
 * - use TIMER0_A0 to bitbang otherwise (MSP430G2452)
 */
#include <stdint.h>
#include <msp430.h>
#include "uart.h"

#ifdef __MSP430_HAS_USCI__

#define WAIT_TX_READY()     while (!(IFG2 & UCA0TXIFG))

void
uart_configure(void)
{
    P1SEL = BIT1 + BIT2 ;  // P1.1 = RXD, P1.2=TXD
    P1SEL2 = BIT1 + BIT2 ; // P1.1 = RXD, P1.2=TXD
    UCA0CTL1 |= UCSSEL_2;  // Use SMCLK
    UCA0BR0 = 104;         // Set baud rate to 9600 with 1MHz clock (Data Sheet 15.3.13)
    UCA0BR1 = 0;           // Set baud rate to 9600 with 1MHz clock
    UCA0MCTL = UCBRS0;     // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;  // Initialize USCI state machine
}

void
uart_putc(uint8_t c)
{
    WAIT_TX_READY();
    UCA0TXBUF = c;
}
#else

#define TXD BIT1 // TXD on P1.1
#define RXD BIT2 // RXD on P1.2

#define FCPU 1000000
#define BAUDRATE 9600

#define BIT_TIME        (FCPU / BAUDRATE)
#define HALF_BIT_TIME   (BIT_TIME / 2)

static uint8_t bitcount;
static uint16_t tx;

inline void uart_configure(void)
{
    P1SEL |= TXD;
    P1DIR |= TXD;
}

void uart_putc(uint8_t c)
{
    tx = c;

    CCTL0 = OUT;             // TXD Idle as Mark
    TACTL = TASSEL_2 + MC_2; // SMCLK, continuous mode

    bitcount = 0xA;          // Load Bit counter, 8 bits + ST/SP
    CCR0 = TAR;              // Initialize compare register

    CCR0 += BIT_TIME;        // Set time till first bit
    tx |= 0x100;             // Add stop bit to TXByte (which is logical 1)
    tx <<= 1;                // Add start bit (which is logical 0)

    CCTL0 = CCIS0 + OUTMOD0 + CCIE; // Set signal, intial value, enable interrupts

    while ( CCTL0 & CCIE );  // Wait for previous TX completion
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A(void)
{
    CCR0 += BIT_TIME; // Add Offset to CCR0
    if (bitcount == 0) { // If all bits TXed
        TACTL = TASSEL_2; // SMCLK, timer off (for power consumption)
        CCTL0 &= ~ CCIE ; // Disable interrupt
    }
    else
    {
        CCTL0 |= OUTMOD2; // Set TX bit to 0
        if (tx & 1)
            CCTL0 &= ~ OUTMOD2; // If it should be 1, set it to 1
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
