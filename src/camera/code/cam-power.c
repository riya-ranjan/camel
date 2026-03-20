#include "rpi.h"
#include "cam-power.h"

enum { CAM_POWER_PIN = 44 };

void cam_power_on(void) {
    gpio_set_output(CAM_POWER_PIN);
    gpio_set_off(CAM_POWER_PIN);
    delay_ms(100);

    gpio_set_on(CAM_POWER_PIN);
    delay_ms(500);
}

void cam_power_off(void) {
    gpio_set_off(CAM_POWER_PIN);
}
