// Test 6: Single-shot button-triggered frame capture.
// Button module (B0DQ3WM7K1) OUT pin on GPIO 27.
// Waits for one button press, captures a frame, saves to SD card, reboots.
#include "rpi.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"
#include "vector-base.h"
#include "gpio.h"
#include "frame.h"

enum { BUTTON_PIN = 27 };

static volatile int button_pressed = 0;

// IRQ handler called from interrupt-asm.S trampoline.
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

    // 1. Initialize camera pipeline + GPU buffer.
    frame_t f;
    frame_init(&f);

    // 2. Initialize SD card.
    frame_sd_init();

    // 3. Configure button pin as input with pull-down.
    gpio_set_input(BUTTON_PIN);
    gpio_set_pulldown(BUTTON_PIN);

    // 4. Set up GPIO rising-edge interrupt on button pin.
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

    // 5. Wait for button press.
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
