/*
 * MSP430 Capacitive Lamp Control
 *
 * Toggling a lamp by touching its plastic base.
 *
 * Description: Basic 1-button input using the built-in pin oscillation feature
 * on GPIO input structure. PinOsc signal feed into TA0CLK. WDT interval is
 * used to gate the measurements. A large difference in measurements indicate
 * button touch.
 *
 * Use an averaging window of SAMPLES as a moving baseline to compare incoming
 * "sensor" readings. This makes a large difference for touch stand out, while
 * allowing for a gradual drift to accomodate for athmospheric changes.
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
 *  window reset indication     |
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
#include <stdlib.h>
#include <msp430.h>
#include "captouch.h"
#include "uart.h"
#include "xprint.h"

uint8_t lamp;
uint8_t state;

uint16_t loops = 0;
int16_t timer = 0;
window_t window;

static void
emit_state(void) {
    xprintf("S%01x p%01x m%04x D%04x A%04x T%04x%04x I%04x,%04x \r\n",
            state,
            lamp,
            timer,
            window.derivative,
            window.avg,
            (uint16_t )(window.total >> 16),
            (uint16_t)(window.total & 0xFFFF),
            window.integral[0],
            window.integral[1]);
}

static void
lamp_on(void)
{
    LED_OUT |= LAMP_PIN;
    lamp = 1;
    emit_state();
}

static void
lamp_off(void)
{
    LED_OUT &= ~LAMP_PIN;
    lamp = 0;
    emit_state();
}

int
main(void)
{
    uint8_t touch;

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
    __no_operation();

    reset_window();
    lamp = 0;
    state = LAMP_RESET;

    for (;;) { /* FOREVER */
        touch = detect();

        switch (state) {
        case LAMP_RESET:
            /* 0. After boot and after a change, take one second to settle */
            LED_OUT |= BOOT_PIN;
            loops++;
            if (loops > LOOPS_1S) {
                loops -= LOOPS_1S;
                LED_OUT &= ~BOOT_PIN;
                state = LAMP_IDLE;
                emit_state();
            }
            break;
                
        case LAMP_IDLE:
            /* 1. In idle, countdown on time or "detect" touch */
            if (lamp) {
                if (timer > 0) {
                    loops++;
                    while (loops > LOOPS_1S) {
                        loops -= LOOPS_1S;
                        timer--;
                        emit_state();
                    }
                } else {
                    lamp_off();
                }
            } else {
                timer = 0;
            }

            if (touch) {
                state = LAMP_ACTION;
            }
            break;

        case LAMP_ACTION:
            /* 2. Change lamp state and revert to RESET */
            lamp ^=1;
            if (lamp) {
                lamp_on();
                timer = AUTO_OFF_S;
            } else {
                lamp_off();
                timer = 0;
            }
            state = LAMP_RESET;
            loops = 0;
            break;
        }
        
        WDTCTL = WDT_DELAY_INTERVAL;
        LPM3;
    }
}

static void
reset_window() {
    uint16_t measurement = measure(TOUCH_PIN);

    window.total = (measurement << SAMPLES_DIV);
    window.last = window.avg = measurement;

    window.derivative = window.integral[0] = window.integral[1] = 0;
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
detect()
{
    uint16_t measurement = measure(TOUCH_PIN);

    /* 
     * Construct an approximate moving average, where some granularity
     * is sacrificed for space 
     */
    window.total -= window.avg;
    window.total += measurement;
    window.avg = (window.total >> SAMPLES_DIV);

    /*
     * Now look at the amount of change
     */
    window.derivative = window.avg - window.last;
    window.last = window.avg;

    /*
     * An increase of capacitance means that less full counts of the
     * oscillator will be counted; so a touch => a negative
     * derivative 
     */
    if (window.derivative < DERIVATIVE_THRESHOLD) {
        window.integral[1] = window.integral[0] - window.derivative;
    } else {
        window.integral[1] = window.integral[0];
    }

    /*
     * Now look at the accumulated change
     */
    if (window.integral[1] > INTEGRAL_THRESHOLD) {
        xprintf("touch %08x\r\n", window.integral[1]);
        emit_state();
        window.integral[0] = window.integral[1] = 0;
        return 1;
    } else {
        /* No touch -- leak away some of our integrand */
        window.integral[0] = (int32_t)(window.integral[1] * LEAKAGE_FACTOR);
        return 0;
    }
}

void __attribute__((interrupt(WDT_VECTOR))) watchdog_timer(void)
{
    LPM3_EXIT;
}
