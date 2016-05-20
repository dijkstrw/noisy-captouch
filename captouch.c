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

/* System Routines*/
static void get_base_count(uint8_t pin);
void measure_count(uint8_t pin);                   // Measures each capacitive sensor

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
    get_base_count(TOUCHPIN);            // Get baseline
    delta_cnt = -1;
    while (delta_cnt < 0) {
        measure_count(TOUCHPIN);              // measure pin oscillator
        delta_cnt = base_cnt - meas_cnt;      // Calculate delta: c_change
        xprintf("%04x-%04x=%04x =>L%04x\r\n",base_cnt,meas_cnt,delta_cnt,lamp);

        // Handle baseline measurement for a baseline decrease
        if (delta_cnt < 0)                        // If delta negative then raw value is larger than baseline
            base_cnt = (base_cnt + meas_cnt) >> 1;  // Update baseline average
    }
    LED_OUT = 0;
    state = LAMP_IDLE;

    for (;;) { /* FOREVER */
        measure_count(TOUCHPIN);
        delta_cnt = base_cnt - meas_cnt;
        xprintf("%04x-%04x=%04x =>L%04x\r\n",base_cnt, meas_cnt, delta_cnt, lamp);

        if (delta_cnt < 0) {
            /* 1. base count update if measured value is larger than our base */
            base_cnt = (base_cnt + meas_cnt) >> 1;
            continue;
        }

        if (delta_cnt > KEY_LVL) {
            /* 2. determine if lamp is touched, if so, remeasure */
            state = LAMP_TOUCHED;
            continue;
        }

        if (state == LAMP_TOUCHED) {
            /* 3. determine if lamp was released, if so wait / debounce one wdt interval */
            state = LAMP_ACTION;
        } else if (state == LAMP_ACTION) {
            /* 4. determine if lamp was released and debounced, if so act */
            lamp ^=1;
            if (lamp)
                LED_OUT |= LED_0;
            else
                LED_OUT &= ~LED_0;
            state = LAMP_IDLE;
        }

        WDTCTL = WDT_delay_setting;
        __bis_SR_register(LPM3_bits);
    }
}

void get_base_count(uint8_t pin) {
    uint8_t i;
    base_cnt = 0;
    for (i = (1 << 4); i > 0; i--) {
        measure_count(pin);
        base_cnt = meas_cnt + base_cnt;
    }
    base_cnt = base_cnt >> 4;
}

void measure_count(uint8_t pin) {
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
    meas_cnt = TACCR1;                      // Save result
    WDTCTL = WDTPW + WDTHOLD;               // Stop watchdog timer
    P2SEL2 &= ~ pin;                        // Disable Pin Oscillator function

}

#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
    __bic_SR_register_on_exit(LPM3_bits);   // Exit LPM3 on reti
}
