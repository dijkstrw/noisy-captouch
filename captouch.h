#ifndef _CAPTOUCH_H
#define _CAPTOUCH_H

/* WDT delays when clocked by fACLK @ 12Khz in ms */
#define WDT_ADLY_2731       (WDTPW+WDTTMSEL+WDTCNTCL+WDTSSEL)
#define WDT_ADLY_683        (WDTPW+WDTTMSEL+WDTCNTCL+WDTSSEL+WDTIS0)
#define WDT_ADLY_43         (WDTPW+WDTTMSEL+WDTCNTCL+WDTSSEL+WDTIS1)
#define WDT_ADLY_5_3        (WDTPW+WDTTMSEL+WDTCNTCL+WDTSSEL+WDTIS1+WDTIS0)

/* Pin definitions */
#define TOUCH_PIN    BIT0
#define LAMP_PIN     BIT0
#define BOOT_PIN     BIT6
#define LED_OUT      P1OUT
#define LED_DIR      P1DIR

/* Watchdog timer intervals, indicating clocks used and time in ms */
#define WDT_MEASURE_INTERVAL WDT_MDLY_8
#define WDT_DELAY_INTERVAL   WDT_ADLY_43

/*
 * Total idle loop = 2 x measure interval + 1 x delay interval
 */
#define LOOPTIME              ((4*8)+43)
#define LOOPS_1S              (1000 / LOOPTIME)

/*
 * Auto turn off in seconds. Set to 0 to disable auto turn off
 */
#define AUTO_OFF_S            3600

#if AUTO_OFF_S > 0xffff
#error Maximum AUTO_OFF_S is 65535
#endif

enum {
    LAMP_RESET = 0,
    LAMP_IDLE,
    LAMP_ACTION
};

/* Sampling window definition */
#define SAMPLES_DIV 4
#define SAMPLES     (1 << SAMPLES_DIV)
struct window {
    uint32_t total;
    uint16_t avg;
    uint8_t current;
    uint8_t startup;
    uint16_t x[SAMPLES];
};
typedef struct window window_t;

static void reset_window(void);
static uint8_t detect(uint8_t pin);
static uint16_t measure(uint8_t pin);

#endif
