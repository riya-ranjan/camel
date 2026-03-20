// single-shot button-triggered frame capture
#include "rpi.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"
#include "vector-base.h"
#include "gpio.h"
#include "frame.h"

enum { BUTTON_PIN = 20 };

static volatile int button_pressed = 0;

void interrupt_vector(unsigned pc) {
    dev_barrier();
    if (gpio_event_detected(BUTTON_PIN)) {
        button_pressed = 1;
        gpio_event_clear(BUTTON_PIN);
    }
    dev_barrier();
}

void notmain(void) {
    output("--- Test 6: Single-Shot Button Capture ---\n");

    frame_t f;
    frame_init(&f);

    frame_sd_init();

    gpio_set_input(BUTTON_PIN);
    gpio_set_pulldown(BUTTON_PIN);

    // set up interrupts
    cpsr_int_disable();
    dev_barrier();
    PUT32(IRQ_Disable_1, 0xffffffff);
    PUT32(IRQ_Disable_2, 0xffffffff);
    dev_barrier();

    extern uint32_t interrupt_vec[];
    vector_base_set(interrupt_vec);

    gpio_int_rising_edge(BUTTON_PIN);
    gpio_event_clear(BUTTON_PIN);
    cpsr_int_enable();

    output("Ready. Press button to capture a photo.\n");

    while (!button_pressed)
        ;

    output("Button pressed! Capturing...\n");
    int r = frame_capture(&f);

    if (r == 0 || r == -3) {
        if (frame_save(&f, "FRAME.RAW"))
            output("Saved FRAME.RAW (%d bytes).\n", FRAME_BUF_SIZE);
        else
            output("ERROR: failed to save.\n");
    } else {
        output("Capture failed (result=%d).\n", r);
    }

    frame_cleanup(&f);
    output("Done.\n");
    clean_reboot();
}
