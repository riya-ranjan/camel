#include "rpi.h"
#include "mbox.h"

uint32_t rpi_temp_get(void) ;

#include "cycle-count.h"

// compute cycles per second using
//  - cycle_cnt_read();
//  - timer_get_usec();
unsigned cyc_per_sec(void) {
    unsigned cyc_start = cycle_cnt_read();
    unsigned s = timer_get_usec();
    while((timer_get_usec() - s) < 1000*1000)
        ;
    unsigned cyc_end = cycle_cnt_read();
    return cyc_end - cyc_start;
}

// testing overclocking:
//  mem write 10000 times  = 139 usec (13.9ns/op)
// read mem 10000 times   = 301 usec (30.1ns / op)
// w/r mem 10000 times    = 461 usec (46.1ns/op)
// GPIO write 10000 times = 188 usec (18.8ns/op)
// read GPIO 10000 times  = 391 usec (39.1ns / op)
// cycles/second       = 1149985640
void test_overclocking(uint32_t clk) {
    // mem write 10000 times
    unsigned s = timer_get_usec();
    char buf[10000];
    for (int i = 0; i < 10000; i++) {
        buf[i] = i * i; 
    }
    unsigned e = timer_get_usec();
    output("mem write 10000 times  = %d usec\n", e - s);

    // read mem 10000 times
    s = timer_get_usec();
    int k = 0;
    for (int i = 0; i < 10000; i++) {
        k += buf[i]; 
    }
    e = timer_get_usec();
    output("read mem 10000 times   = %d usec\n", e - s);

    // w/r mem 10000 times
    s = timer_get_usec();
    for (int i = 0; i < 10000; i++) {
        buf[i] += i; 
    }
    e = timer_get_usec();
    output("w/r mem 10000 times    = %d usec\n", e - s);
}



void notmain(void) { 
    output("mailbox serial number = %llx\n", rpi_get_serialnum());
    // todo("implement the rest");

    output("mailbox revision number = %x\n", rpi_get_revision());
    output("mailbox model number = %x\n", rpi_get_model());

    uint32_t size = rpi_get_memsize();
    output("mailbox physical mem: size=%d (%dMB)\n", 
            size, 
            size/(1024*1024));

    // print as fahrenheit
    unsigned x = rpi_temp_get();

    // // convert <x> to C and F
    unsigned C = x / 1000, F = C * 9 / 5 + 32;
    output("mailbox temp = %x, C=%d F=%d\n", x, C, F); 

    // todo("do overclocking!\n");
    unsigned arm_clock = 0x000000004;
    uint32_t arm_current = rpi_clock_curhz_get(arm_clock);
    uint32_t arm_max = rpi_clock_maxhz_get(arm_clock);
    uint32_t arm_real = rpi_clock_realhz_get(arm_clock);

    output("mailbox arm clock: cur=%d max=%d real=%d\n", 
            arm_current, arm_max, arm_real);
    test_overclocking(arm_clock);
    uint32_t arm_reset = rpi_clock_hz_set(arm_clock, arm_max);
    arm_current = rpi_clock_curhz_get(arm_clock);
    output("mailbox arm clock: reset=%d\n", arm_current);
    test_overclocking(arm_clock);
}


