#ifndef _CAPTOUCH_H
#define _CAPTOUCH_H

/* WDT delays when clocked by fACLK @ 12Khz in ms */
#define WDT_ADLY_2731        (WDTPW+WDTTMSEL+WDTCNTCL+WDTSSEL)
#define WDT_ADLY_683         (WDTPW+WDTTMSEL+WDTCNTCL+WDTSSEL+WDTIS0)
#define WDT_ADLY_43          (WDTPW+WDTTMSEL+WDTCNTCL+WDTSSEL+WDTIS1)
#define WDT_ADLY_5_3         (WDTPW+WDTTMSEL+WDTCNTCL+WDTSSEL+WDTIS1+WDTIS0)

/* Pin definitions */
#define TOUCH_PIN            BIT0
#define LAMP_PIN             BIT0
#define BOOT_PIN             BIT6
#define LED_OUT              P1OUT
#define LED_DIR              P1DIR

/* Watchdog timer intervals, indicating clocks used and time in ms */
#define WDT_MEASURE_INTERVAL WDT_MDLY_8
#define WDT_DELAY_INTERVAL   WDT_ADLY_5_3

/*
 * Total idle loop = 2 x measure interval + 1 x delay interval
 */
#define LOOPTIME             ((2*8) + 5)
#define LOOPS_1S             (1000 / LOOPTIME)

/*
 * Auto turn off in seconds. Set to 0 to disable auto turn off
 */
#define AUTO_OFF_S           (20*60)

#if AUTO_OFF_S > 0xffff
#error Maximum AUTO_OFF_S is 65535
#endif

enum {
    LAMP_RESET = 0,
    LAMP_IDLE,
    LAMP_ACTION
};

/* Sampling window definition 
 *
 * The sampling window is 1 << SAMPLES_DIV items large. This window is
 * used to calculate a moving average. The moving average must differ
 * DERIVATIVE_THRESHOLD from the last value in order to count. At that
 * point the value is added to an integrand. This integrand is
 * compared with the INTEGRAL_THRESHOLD to determine if a touch
 * happened. When a touch did not happen, the integrand is updated
 * with LEAKAGE_FACTOR to ensure it slowly drains away.
 */
#define SAMPLES_DIV          5
#define DERIVATIVE_THRESHOLD (int16_t)(-8)
#define INTEGRAL_THRESHOLD   0x20
#define LEAKAGE_FACTOR       0.80f

struct window {
    int32_t total;
    int16_t avg;
    int16_t last;
    int16_t derivative;
    int16_t integral[2];
};
typedef struct window window_t;

static void reset_window(void);
static uint8_t detect();
static uint16_t measure(uint8_t pin);

#endif
