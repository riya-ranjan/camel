#include "rpi.h"
#include "cam-power.h"

// Pi Zero W: CAMERA_0_SHUTDOWN = GPIO 44 (configured via dt-blob.bin).
// The firmware holds GPIO 44 LOW at boot (camera shut down).
// A clean LOW→HIGH edge is required to enable the camera board's
// regulators, 24MHz oscillator, and release the IMX477 XCLR pin.
enum { CAM_POWER_PIN = 44 };

void cam_power_on(void) {
    // Ensure clean shutdown first.
    gpio_set_output(CAM_POWER_PIN);
    gpio_set_off(CAM_POWER_PIN);
    delay_ms(100);

    // Power on: drive HIGH.
    gpio_set_on(CAM_POWER_PIN);
    delay_ms(500);   // wait for regulators + oscillator + sensor boot
}

void cam_power_off(void) {
    gpio_set_off(CAM_POWER_PIN);
}
