// no parthiv board, so use GPIO output to power button
#include "rpi.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"
#include "vector-base.h"
#include "gpio.h"
#include "frame.h"
#include "raw2lcd.h"
#include "ili9486.h"

enum {
    BUTTON_POWER_PIN  = 21,   // GPIO output HIGH → supplies 3.3V to button
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

void notmain(void) {
    output("--- Test 10: Button Capture + LCD (no Parthiv) ---\n");

    frame_t f;
    frame_init(&f);

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

    output("Ready. Press button to capture.\n");

    wb_gains_t wb = WB_INDOOR_2848K;
    unsigned n = 0;

    while (1) {
        while (!button_pressed)
            ;
        cpsr_int_disable();

        output("Capture %d...\n", n);
        uint32_t t0 = timer_get_usec();
        int r = frame_capture(&f);
        uint32_t t1 = timer_get_usec();
        output("  capture: %d us (result=%d)\n", t1 - t0, r);

        if (r == 0 || r == -3) {
            t0 = timer_get_usec();
            raw2lcd_display(&f, &wb, NULL, NULL);
            t1 = timer_get_usec();
            output("  display: %d us\n", t1 - t0);
        } else {
            output("  FAILED\n");
        }

        n++;

        delay_ms(200);
        gpio_event_clear(BUTTON_SIGNAL_PIN);
        button_pressed = 0;
        cpsr_int_enable();
    }
}
