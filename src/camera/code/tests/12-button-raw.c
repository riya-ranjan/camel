// same as 11 but save RAW instead of jpeg
#include "rpi.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"
#include "vector-base.h"
#include "gpio.h"
#include "frame.h"
#include "raw2lcd.h"
#include "ili9486.h"

enum {
    BUTTON_POWER_PIN  = 21,
    BUTTON_SIGNAL_PIN = 20,
};

static volatile int button_pressed = 0;

void interrupt_vector(unsigned pc) {
    dev_barrier();
    if (gpio_event_detected(BUTTON_SIGNAL_PIN)) {
        button_pressed = 1;
        gpio_event_clear(BUTTON_SIGNAL_PIN);
    }
    dev_barrier();
}

static void make_filename(char *buf, unsigned n) {
    buf[0] = 'R'; buf[1] = 'A'; buf[2] = 'W';
    buf[3] = '0' + (n / 10) % 10;
    buf[4] = '0' + n % 10;
    buf[5] = '.'; buf[6] = 'R'; buf[7] = 'A'; buf[8] = 'W';
    buf[9] = '\0';
}

void notmain(void) {
    output("--- Test 12: Button -> LCD -> RAW Save ---\n");

    frame_t f;
    frame_init(&f);

    frame_sd_init();

    ili9486_hal_setup();
    raw2lcd_init();
    ili9486_fill_screen(ILI9486_BLACK);
    output("LCD ready.\n");

    gpio_set_output(BUTTON_POWER_PIN);
    gpio_write(BUTTON_POWER_PIN, 1);

    gpio_set_input(BUTTON_SIGNAL_PIN);
    gpio_set_pulldown(BUTTON_SIGNAL_PIN);

    cpsr_int_disable();
    dev_barrier();
    PUT32(IRQ_Disable_1, 0xffffffff);
    PUT32(IRQ_Disable_2, 0xffffffff);
    dev_barrier();

    extern uint32_t interrupt_vec[];
    vector_base_set(interrupt_vec);

    gpio_int_rising_edge(BUTTON_SIGNAL_PIN);
    gpio_event_clear(BUTTON_SIGNAL_PIN);
    cpsr_int_enable();

    output("Ready. Press button to capture + save.\n");

    wb_gains_t wb = WB_INDOOR_2848K;
    unsigned n = 0;

    while (1) {
        while (!button_pressed)
            ;
        cpsr_int_disable();

        output("--- Capture %d ---\n", n);
        uint32_t total_t0 = timer_get_usec();

        uint32_t t0 = timer_get_usec();
        int r = frame_capture(&f);
        uint32_t t1 = timer_get_usec();
        output("  capture: %d us (result=%d)\n", t1 - t0, r);

        if (r != 0 && r != -3) {
            output("  CAPTURE FAILED, skipping.\n");
            n++;
            delay_ms(200);
            gpio_event_clear(BUTTON_SIGNAL_PIN);
            button_pressed = 0;
            cpsr_int_enable();
            continue;
        }

        t0 = timer_get_usec();
        raw2lcd_display(&f, &wb, NULL, NULL);
        t1 = timer_get_usec();
        output("  display: %d us\n", t1 - t0);

        char fname[12];
        make_filename(fname, n);
        output("  saving %s ...\n", fname);

        t0 = timer_get_usec();
        int ok = frame_save(&f, fname);
        t1 = timer_get_usec();
        output("  raw save: %d us (%s)\n", t1 - t0, ok ? "OK" : "FAILED");

        uint32_t total_t1 = timer_get_usec();
        output("  TOTAL: %d us\n", total_t1 - total_t0);

        n++;
        delay_ms(200);
        gpio_event_clear(BUTTON_SIGNAL_PIN);
        button_pressed = 0;
        cpsr_int_enable();
    }
}
