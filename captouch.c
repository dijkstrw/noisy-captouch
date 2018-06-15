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
#include <msp430.h>
#include "captouch.h"
#include "uart.h"
#include "xprint.h"

/*
 * Pin oscillations are counted on the capacitive touch pin. If more
 * capacitance is present, the frequency measured (the number of pin
 * oscillations seen) drop. This defines by how much they must drop when
 * compared to the moving average before we indicate that a person has keyed
 * the sensor.
 */
#define KEY_LEVEL       0x200

uint8_t lamp;
uint8_t state;

uint16_t loops = 0;
int16_t timer = 0;
window_t window;

static void
lamp_on(void)
{
    LED_OUT |= LAMP_PIN;
    lamp = 1;
}

static void
lamp_off(void)
{
    LED_OUT &= ~LAMP_PIN;
    lamp = 0;
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

    reset_window();
    lamp = 0;
    state = LAMP_RESET;

    for (;;) { /* FOREVER */
        switch (state) {
        case LAMP_RESET:
            /* 0. Reset measurement window to ensure correct detection for next touch */
            if (window.startup) {
                LED_OUT |= BOOT_PIN;
                detect(TOUCH_PIN);
                xprintf("%04x-%04x=%04x =>L%01x\r\n",
                        window.avg,
                        window.x[window.current],
                        (window.avg - window.x[window.current]),
                        lamp);
                LED_OUT &= ~BOOT_PIN;
                continue;
            } else {
                state = LAMP_IDLE;
            }
            break;

        case LAMP_IDLE:
            /* 1. Determine if lamp is touched */
            touch = detect(TOUCH_PIN);
            if (lamp) {
                if (timer > 0) {
                    loops++;
                    while (loops > LOOPS_1S) {
                        loops -= LOOPS_1S;
                        timer--;
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
            /* 2. Actuate */
            lamp ^=1;
            if (lamp) {
                lamp_on();
                timer = AUTO_OFF_S;
            } else {
                lamp_off();
                timer = 0;
            }
            reset_window();
            state = LAMP_RESET;
            break;
        }


        xprintf("S%01x %04x: %04x-%04x=%04x =>L%01x\r\n",
                state,
                timer,
                window.avg,
                window.x[window.current],
                (window.avg - window.x[window.current]),
                lamp);

        WDTCTL = WDT_DELAY_INTERVAL;
        LPM3;
    }
}

static void
reset_window() {
    window.startup = 1;
    window.current = 0;
    window.total = 0;
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

        window.total -= window.x[c];
        window.x[c] = m;
        window.total += m;
        window.avg = (window.total >> SAMPLES_DIV);

        if (delta > KEY_LEVEL) {
            xprintf("D%04x\n\r", delta);
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
