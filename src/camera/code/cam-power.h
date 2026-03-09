#ifndef __CAM_POWER_H__
#define __CAM_POWER_H__

// Camera board power control via GPIO 44.
// The HQ Camera board's regulator is enabled by driving GPIO 44 high.

void cam_power_on(void);
void cam_power_off(void);

#endif
