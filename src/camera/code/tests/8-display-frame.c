// capture a frame and display it on the ILI9486 LCD.
#include "rpi.h"
#include "frame.h"
#include "raw2lcd.h"
#include "ili9486.h"

void notmain(void)
{
    output("--- Test 8: Display Frame on LCD ---\n");

    frame_t f;
    frame_init(&f);

    ili9486_hal_setup();
    raw2lcd_init();
    ili9486_fill_screen(ILI9486_BLACK);
    output("LCD ready (480x320 landscape).\n");

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

    output("Displaying...\n");
    wb_gains_t wb = WB_INDOOR_2848K;
    t0 = timer_get_usec();
    raw2lcd_display(&f, &wb, NULL, NULL);           // convert output but not to Jpeg
    t1 = timer_get_usec();
    output("Display: %d us\n", t1 - t0);

    output("Done.\n");
    frame_cleanup(&f);
    while (1) {
        output("Press reset to exit.\n");
    }
}
