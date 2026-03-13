// Test 8: Capture a frame and display it on the ILI9486 LCD.
// Camera -> RAW12 -> on-device ISP (downscale + demosaic + WB + gamma) -> LCD.
#include "rpi.h"
#include "frame.h"
#include "raw2lcd.h"
#include "ili9486.h"

void notmain(void)
{
    output("--- Test 8: Display Frame on LCD ---\n");

    // 1. Initialize camera pipeline + allocate GPU DMA buffer.
    frame_t f;
    frame_init(&f);

    // 2. Initialize LCD (SPI, GPIO, display controller).
    ili9486_hal_setup();
    raw2lcd_init();
    ili9486_fill_screen(ILI9486_BLACK);
    output("LCD ready (480x320 landscape).\n");

    // 3. Capture one frame.
    output("Capturing frame...\n");
    uint32_t t0 = timer_get_usec();
    int r = frame_capture(&f);
    uint32_t t1 = timer_get_usec();
    output("Capture: %d us (result=%d)\n", t1 - t0, r);

    if (r != 0 && r != -3) {
        output("Capture failed!\n");
        frame_cleanup(&f);
        return;
    }

    // 4. Process and display on LCD.
    output("Displaying...\n");
    wb_gains_t wb = WB_INDOOR_2848K;
    t0 = timer_get_usec();
    raw2lcd_display(&f, &wb);
    t1 = timer_get_usec();
    output("Display: %d us\n", t1 - t0);

    output("Done.\n");
    frame_cleanup(&f);
    while (1) {
        output("Press button to exit.\n");
    }
}
