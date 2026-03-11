// Test 7: Looping button-triggered frame capture.
// Button module (B0DQ3WM7K1) OUT pin on GPIO 27.
// Each button press captures a frame and saves it to SD card.
// Files: FRM0000.RAW, FRM0001.RAW, ... up to MAX_CAPTURES.
#include "rpi.h"
#include "rpi-interrupts.h"
#include "rpi-inline-asm.h"
#include "vector-base.h"
#include "gpio.h"
#include "frame.h"

enum {
    BUTTON_PIN   = 27,
    MAX_CAPTURES = 100,
};

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

// Build 8.3 filename: "FRM0000.RAW" through "FRM0099.RAW".
// FAT32 valid chars: A-Z, 0-9 only (no underscores).
static void make_filename(char *buf, unsigned n) {
    buf[0] = 'F'; buf[1] = 'R'; buf[2] = 'M';
    buf[3] = '0' + (n / 1000) % 10;
    buf[4] = '0' + (n / 100) % 10;
    buf[5] = '0' + (n / 10) % 10;
    buf[6] = '0' + n % 10;
    buf[7] = '.';
    buf[8] = 'R'; buf[9] = 'A'; buf[10] = 'W';
    buf[11] = 0;
}

void notmain(void) {
    output("--- Test 7: Button Capture Loop ---\n");

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

    output("Ready. Press button to capture (%d max).\n", MAX_CAPTURES);

    // 5. Main loop: wait for button press, capture, save.
    for (unsigned n = 0; n < MAX_CAPTURES; n++) {
        while (!button_pressed)
            ;
        button_pressed = 0;
        output("Capturing frame %d...\n", n);

        int r = frame_capture(&f);

        if (r == 0 || r == -3) {
            char filename[12];
            make_filename(filename, n);
            if (frame_save(&f, filename))
                output("Saved %s (%d bytes).\n", filename, FRAME_BUF_SIZE);
            else
                output("ERROR: failed to save %s.\n", filename);
        } else {
            output("Capture failed (result=%d).\n", r);
        }

        // Debounce: wait 200ms, clear any stale events.
        delay_ms(200);
        gpio_event_clear(BUTTON_PIN);
    }

    output("Reached %d captures. Done.\n", MAX_CAPTURES);
    frame_cleanup(&f);
    clean_reboot();
}
