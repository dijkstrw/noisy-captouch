//******************************************************************************
//  MSP430G2553 Demo - Capacitive Touch, Pin Oscillator Method, 1 button
//
//  Description: Basic 1-button input using the built-in pin oscillation feature
//  on GPIO input structure. PinOsc signal feed into TA0CLK. WDT interval is used
//  to gate the measurements. Difference in measurements indicate button touch.
//
//  ACLK = VLO = 12kHz, MCLK = SMCLK = 1MHz DCO
//
//               MSP430G2xx3
//             -----------------
//         /|\|              XIN|-
//          | |                 |
//          --|RST          XOUT|-
//            |                 |
//            |             P2.0|<--Capacitive Touch Input 1
//            |                 |
//  LED 2  <--|P1.6             |
//            |                 |
//  LED 1  <--|P1.0             |
//            |                 |
//            |                 |
//
//  Based on source from:
//  Benn Thomsen
//  March 2016
//
//  in turn based on:
//  Brandon Elliott/D. Dang
//  Texas Instruments Inc.
//  November 2010
//******************************************************************************

#include <stdint.h>
#include <msp430.h>
#include "uart.h"
#include "xprint.h"

#define TOUCHPIN BIT0   //P2.0
#define LED_0 BIT0
#define LED_1 BIT6

#define LED_OUT P1OUT
#define LED_DIR P1DIR

/* Define User Configuration values */
/* Defines WDT SMCLK interval for sensor measurements*/
#define WDT_meas_setting (DIV_SMCLK_8192)
/* Defines WDT ACLK interval for delay between measurement cycles*/
#define WDT_delay_setting (DIV_ACLK_512)

/* Sensor settings*/
#define KEY_LVL     600                     // Defines threshold for a key press

/* Definitions for use with the WDT settings*/
#define DIV_ACLK_32768  (WDT_ADLY_1000)     // ACLK/32768
#define DIV_ACLK_8192   (WDT_ADLY_250)      // ACLK/8192
#define DIV_ACLK_512    (WDT_ADLY_16)       // ACLK/512
#define DIV_ACLK_64     (WDT_ADLY_1_9)      // ACLK/64
#define DIV_SMCLK_32768 (WDT_MDLY_32)       // SMCLK/32768
#define DIV_SMCLK_8192  (WDT_MDLY_8)        // SMCLK/8192
#define DIV_SMCLK_512   (WDT_MDLY_0_5)      // SMCLK/512
#define DIV_SMCLK_64    (WDT_MDLY_0_064)    // SMCLK/64

enum {
    LAMP_IDLE = 0,
    LAMP_TOUCHED,
    LAMP_ACTION
};

// Global variables for sensing
uint16_t base_cnt, meas_cnt;
int16_t delta_cnt, j;
uint8_t lamp = 0;
uint8_t state = LAMP_IDLE;

#define SAMPLES_DIV 4
#define SAMPLES     (1 << SAMPLES_DIV)

struct window {
    uint32_t total;
    uint16_t avg;
    uint8_t current;
    uint8_t startup;
    uint16_t x[SAMPLES];
} window;

static uint8_t detect(uint8_t pin);

int main(void) {
    WDTCTL = WDTPW + WDTHOLD;    // Stop WDT
    DCOCTL = 0;                  // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;       // Set DCO to 1MHz
    DCOCTL = CALDCO_1MHZ;
    BCSCTL3 |= LFXT1S_2;         // LFXT1 = VLO 12kHz

    IE1 |= WDTIE;                // enable WDT interrupt

    LED_DIR |= (LED_0 + LED_1);
    LED_OUT &= ~(LED_0 + LED_1);

    uart_configure();             // Initialise UART for serial comms
    __bis_SR_register(GIE);      // Enable interrupts

    LED_OUT |= LED_1;
    window.startup = 1;
    window.current = 0;
    window.total = 0;

    while (window.startup) {
        detect(TOUCHPIN);
        xprintf("%04x-%04x=%04x =>L%04x\r\n",
                window.avg,
                window.x[window.current],
                (window.avg - window.x[window.current]),
                lamp);
    }
    LED_OUT = 0;
    state = LAMP_IDLE;

    for (;;) { /* FOREVER */
        if (detect(TOUCHPIN)) {
            /* 1. determine if lamp is touched, if so, remeasure */
            state = LAMP_TOUCHED;
            continue;
        }

        if (state == LAMP_TOUCHED) {
            /* 2. determine if lamp was released, if so wait / debounce one wdt interval */
            state = LAMP_ACTION;
        } else if (state == LAMP_ACTION) {
            /* 3. determine if lamp was released and debounced, if so act */
            lamp ^=1;
            if (lamp)
                LED_OUT |= LED_0;
            else
                LED_OUT &= ~LED_0;
            state = LAMP_IDLE;
        }

        xprintf("%04x-%04x=%04x =>L%04x\r\n",
                window.avg,
                window.x[window.current],
                (window.avg - window.x[window.current]),
                lamp);

        WDTCTL = WDT_delay_setting;
        __bis_SR_register(LPM3_bits);
    }
}

static uint16_t
measure(uint8_t pin) {
    uint16_t result;

    /* Ground the input */
    P2SEL &= ~pin;
    P2DIR &= ~pin;
    P2OUT &= ~pin;
    P2REN |= pin;
    P2REN &= ~pin;
    WDTCTL = WDT_meas_setting;              // Start WDT, Clock Source: ACLK, Interval timer
    __bis_SR_register(LPM0_bits + GIE);     // Wait for WDT interrupt

    TA0CTL = TASSEL_3 + MC_2;           // INCLK from Pin oscillator, continous count mode to 0xFFFF
    TA0CCTL1 = CM_3 + CCIS_2 + CAP;     // Capture on Rising and Falling Edge,Capture input GND,Capture mode


    /*Configure Ports for relaxation oscillator*/
    /*The P2SEL2 register allows Timer_A to receive it's clock from a GPIO*/
    /*See the Application Information section of the device datasheet for info*/
    P2DIR &= ~ pin;                    // P2.0 is the input used here
    P2SEL &= ~ pin;
    P2SEL2 |= pin;

    /*Setup Gate Timer*/
    WDTCTL = WDT_meas_setting;              // Start WDT, Clock Source: ACLK, Interval timer
    TA0CTL |= TACLR;                        // Reset Timer_A (TAR) to zero
    __bis_SR_register(LPM0_bits + GIE);     // Wait for WDT interrupt
    TA0CCTL1 ^= CCIS0;                      // Toggle the counter capture input to capture count using TACCR1
    result = TACCR1;
    WDTCTL = WDTPW + WDTHOLD;               // Stop watchdog timer
    P2SEL2 &= ~ pin;                        // Disable Pin Oscillator function

    return result;
}

static uint8_t
detect(uint8_t pin)
{
    uint8_t c = window.current;
    uint16_t m = measure(pin);
    int16_t delta;

    c = (c + 1) % SAMPLES;
    window.current = c;

    if (window.startup) {
        window.x[c] = m;
        window.total += window.x[c];
        if (c == 0) {
            window.startup = 0;
            window.avg = (window.total >> SAMPLES_DIV);
        }
    } else {
        delta = window.avg - m;

        if ((-0xff < delta) && (delta < 0xff)) {
            window.total -= window.x[c];
            window.x[c] = m;
            window.total += m;
            window.avg = (window.total >> SAMPLES_DIV);
        }

        if (delta > KEY_LVL)
            return 1;
    }

    return 0;
}

#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
    __bic_SR_register_on_exit(LPM3_bits);   // Exit LPM3 on reti
}
