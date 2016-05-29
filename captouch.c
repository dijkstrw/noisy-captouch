/*
 * MSP430 Capacitive Lamp Control
 *
 * Toggling a lamp by touching its plastic base.
 *
 * Description: Basic 1-button input using the built-in pin oscillation feature
 * on GPIO input structure. PinOsc signal feed into TA0CLK. WDT interval is
 * used to gate the measurements. Difference in measurements indicate button
 * touch.
 *
 * Use an averaging window of SAMPLES as a moving baseline to compare incoming
 * "sensor" readings. The baseline has notion of a maximum drift level that
 * allows for small variations in sensor reading because of environmental
 * changes. E.g. Athmospheric changes will cause false positives due to drift
 * otherwise.
 *
 * ACLK = VLO = 12kHz, MCLK = SMCLK = 1MHz DCO
 *
 *               MSP430G2xxx
 *             -----------------
 *         /|\|              XIN|- UART
 *          | |                 |
 *          --|RST          XOUT|-
 *            |                 |
 *            |             P2.0|<--Capacitive Touch Input 1
 *            |                 |
 *  LED 2  <--|P1.6             |
 *  boot indication             |
 *  LED 1  <--|P1.0             |
 *  lamp driver/indication      |
 *            |                 |
 *
 *
 * Based on source from:
 * - Benn Thomsen, March 2016
 * - Brandon Elliott/D. Dang, Texas Instruments Inc., November 2010
 */

#include <stdint.h>
#include <msp430.h>
#include "captouch.h"
#include "uart.h"
#include "xprint.h"

/*
 * Sensor settings
 *
 * The sensor needs to sense
 * - large capacitance changes (to ground) -> which indicate a presence
 * - small changes due to atmospheric conditions
 *
 * The large changes are called the KEY level below. Anything small,
 * i.e. smaller than the DRIFT level is used to adjust the comparison
 * window. Items between DRIFT and KEY are ignored.
 *
 * Note finally that drifting is bound in the positive direction, but not in
 * the negative direction. This allows our average to quickly recover to a more
 * sensitve position.
 */
#define MAX_DRIFT_LEVEL 0xff
#define KEY_LEVEL       0x260

uint8_t lamp = 0;
uint8_t state = LAMP_IDLE;

uint16_t loops = 0;
uint16_t timer = 0;
window_t window;

static void
lamp_on(void)
{
    LED_OUT |= LAMP_PIN;
    timer = AUTO_OFF_S;
    lamp = 1;
}

static void
lamp_off(void)
{
    LED_OUT &= ~LAMP_PIN;
    timer = 0;
    lamp = 0;
}

int
main(void)
{
    WDTCTL = WDTPW + WDTHOLD;    /* Stop WDT */
    DCOCTL = 0;                  /* Select lowest DCOx and MODx settings */
    BCSCTL1 = CALBC1_1MHZ;       /* Set DCO to 1MHz */
    DCOCTL = CALDCO_1MHZ;
    BCSCTL3 |= LFXT1S_2;         /* LFXT1 = VLO 12kHz */

    IE1 |= WDTIE;

    LED_DIR |= (LAMP_PIN + BOOT_PIN);
    LED_OUT &= ~(LAMP_PIN + BOOT_PIN);

    uart_configure();
    _BIS_SR(GIE);

    LED_OUT |= BOOT_PIN;
    window.startup = 1;
    window.current = 0;
    window.total = 0;

    while (window.startup) {
        detect(TOUCH_PIN);
        xprintf("%04x-%04x=%04x =>L%01x\r\n",
                window.avg,
                window.x[window.current],
                (window.avg - window.x[window.current]),
                lamp);
    }
    LED_OUT = 0;
    state = LAMP_IDLE;

    for (;;) { /* FOREVER */
        if (detect(TOUCH_PIN)) {
            /* 1. determine if lamp is touched, if so, remeasure */
            state = LAMP_TOUCHED;
            continue;
        }
        if (state == LAMP_TOUCHED) {
            /* 2. determine if lamp was released, if so wait / debounce one wdt interval */
            state = LAMP_ACTION;
        } else if (state == LAMP_ACTION) {
            /* 3. debounced, actuate */
            lamp ^=1;
            if (lamp)
                lamp_on();
            else
                lamp_off();
            state = LAMP_IDLE;
        }
        if ((state == LAMP_IDLE) && timer) {
            loops++;
            while (loops > LOOPS_1S) {
                loops -= LOOPS_1S;
                timer--;
            }
            if (timer == 0)
                lamp_off();
        }

        xprintf("%04x: %04x-%04x=%04x =>L%01x\r\n",
                timer,
                window.avg,
                window.x[window.current],
                (window.avg - window.x[window.current]),
                lamp);

        WDTCTL = WDT_DELAY_INTERVAL;
        LPM3;
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
    WDTCTL = WDT_MEASURE_INTERVAL;
    _BIS_SR(LPM0_bits + GIE);

    /* Configure timer to capture oscillator */
    TA0CTL = TASSEL_3 + MC_2;           /* INCLK from Pin oscillator, continous count mode to 0xFFFF */
    TA0CCTL1 = CM_3 + CCIS_2 + CAP;     /* Capture on Rising and Falling Edge,Capture input GND,Capture mode */


    /*
     * Configure Ports for relaxation oscillator
     * P2SEL2 allows Timer_A to receive it's clock from a GPIO
     */
    P2DIR &= ~ pin;
    P2SEL &= ~ pin;
    P2SEL2 |= pin;

    /* Setup Gate Timer */
    WDTCTL = WDT_MEASURE_INTERVAL;
    TA0CTL |= TACLR;                    /* Reset timer */
    _BIS_SR(LPM0_bits + GIE);
    TA0CCTL1 ^= CCIS0;                  /* Toggle the counter capture input to capture count using TACCR1 */
    result = TACCR1;
    WDTCTL = WDTPW + WDTHOLD;
    P2SEL2 &= ~ pin;                    /* Disable Oscillator */

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

        if (delta < MAX_DRIFT_LEVEL) {
            window.total -= window.x[c];
            window.x[c] = m;
            window.total += m;
            window.avg = (window.total >> SAMPLES_DIV);
        }

        if (delta > KEY_LEVEL) {
            xprintf("S%04x\n\r", delta);
            return 1;
        }
    }

    return 0;
}

#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
    LPM3_EXIT;
}
