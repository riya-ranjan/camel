/*
 * Implement the following routines to set GPIO pins to input or 
 * output, and to read (input) and write (output) them.
 *  1. DO NOT USE loads and stores directly: only use GET32 and 
 *    PUT32 to read and write memory.  See <start.S> for thier
 *    definitions.
 *  2. DO USE the minimum number of such calls.
 * (Both of these matter for the next lab.)
 *
 * See <rpi.h> in this directory for the definitions.
 *  - we use <gpio_panic> to try to catch errors.  For lab 2
 *    it only infinite loops since we don't have <printk>
 */
#include "rpi.h"
#include "gpio.h"

// See broadcomm documents for magic addresses and magic values.
//
// If you pass addresses as:
//  - pointers use put32/get32.
//  - integers: use PUT32/GET32.
//  semantics are the same.
enum {
    // Max gpio pin number.
    GPIO_MAX_PIN = 53,

    GPIO_BASE = 0x20200000,
    gpio_set0  = (GPIO_BASE + 0x1C),
    gpio_clr0  = (GPIO_BASE + 0x28),
    gpio_lev0  = (GPIO_BASE + 0x34),

    // <you will need other values from BCM2835!>
    gpio_ren0 = (GPIO_BASE + 0x4c),
    gpio_fen0 = (GPIO_BASE + 0x58),
    gpio_eds0 = (GPIO_BASE + 0x40),
    ENABLE_IRQS_2 = 0x2000b214
};

//
// Part 1 implement gpio_set_on, gpio_set_off, gpio_set_output
//

// set <pin> to be an output pin.
//
// NOTE: fsel0, fsel1, fsel2 are contiguous in memory, so you
// can (and should) use ptr calculations versus if-statements!
void gpio_set_output(unsigned pin) {
    gpio_set_function(pin, GPIO_FUNC_OUTPUT);
    // if(pin > GPIO_MAX_PIN)
    //     gpio_panic("illegal pin=%d\n", pin);
    // unsigned int register_n = pin / 10;
    // volatile unsigned int *gpio_fsel = (volatile unsigned int *)GPIO_BASE;
    // unsigned int value = get32(gpio_fsel + register_n);
    // unsigned int fsel_register_offset = (pin % 10)*3;
    // unsigned int mask = 0x7 << fsel_register_offset;
    // value &= ~mask;
    // value |= 0x1 << fsel_register_offset;
    // put32(gpio_fsel + register_n, value);
}

// Set GPIO <pin> = on.
void gpio_set_on(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    volatile unsigned int *gpio_set = (volatile unsigned int *)gpio_set0;
    unsigned int register_n = pin / 32;
    unsigned int register_offset = pin % 32;
    put32(gpio_set + register_n, (0b1 << register_offset));
}

// Set GPIO <pin> = off
void gpio_set_off(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    volatile unsigned int *gpio_clr = (volatile unsigned int *)gpio_clr0;
    unsigned int register_n = pin / 32;
    unsigned int register_offset = pin % 32;
    put32(gpio_clr + register_n, (0b1 << register_offset));
}

// Set <pin> to <v> (v \in {0,1})
void gpio_write(unsigned pin, unsigned v) {
    if(v)
        gpio_set_on(pin);
    else
        gpio_set_off(pin);
}

//
// Part 2: implement gpio_set_input and gpio_read
//

// set <pin> = input.
void gpio_set_input(unsigned pin) {
    gpio_set_function(pin, GPIO_FUNC_INPUT);
    // if(pin > GPIO_MAX_PIN)
    //     gpio_panic("illegal pin=%d\n", pin);
    // unsigned int register_n = pin / 10;
    // volatile unsigned int *gpio_fsel = (volatile unsigned int *)GPIO_BASE;
    // unsigned int value = get32(gpio_fsel + register_n);
    // unsigned int fsel_register_offset = (pin % 10)*3;
    // unsigned int mask = 0x7 << fsel_register_offset;
    // value &= ~mask;
    // put32(gpio_fsel + register_n, value);
}

// Return 1 if <pin> is on, 0 if not.
int gpio_read(unsigned pin) {
    unsigned v = 0;

    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    volatile unsigned int *gpio_lev = (volatile unsigned int *)gpio_lev0;
    unsigned int register_n = pin / 32;
    unsigned int register_offset = pin % 32;
    v = (get32(gpio_lev + register_n) >> register_offset) & 0x1;
    return v;
}

// set GPIO function for <pin> (input, output, alt...).
// settings for other pins should be unchanged.
void gpio_set_function(unsigned pin, gpio_func_t function) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    if((function & 0b111) != function)
        gpio_panic("illegal func=%x\n", function); 

    unsigned int register_n = pin / 10;
    volatile unsigned int *gpio_fsel = (volatile unsigned int *)GPIO_BASE;
    unsigned int value = get32(gpio_fsel + register_n);
    unsigned int fsel_register_offset = (pin % 10)*3;
    unsigned int mask = 0x7 << fsel_register_offset;
    value &= ~mask;
    value |= function << fsel_register_offset;
    put32(gpio_fsel + register_n, value);
    
}

// int gpio_has_interrupt(void) {
    
// }

// void gpio_int_rising_edge(unsigned pin) {
//     dev_barrier();
//     if (pin > 32) 
//         gpio_panic("illegal pin for interrupts, pin=%d\n", pin);

//     // enable interrupt for GPIO_INT0
//     OR32(ENABLE_IRQS_2, 1 << (49 - 32));
//     // using p97 documentation
//     dev_barrier();
//     OR32(gpio_ren0, 1 << (pin % 32));
//     dev_barrier();
// }

// void gpio_int_falling_edge(unsigned pin) {
//     dev_barrier();
//     if (pin > 32) 
//         gpio_panic("illegal pin for interrupts, pin=%d\n", pin);

//     // enable interrupt for GPIO_INT0
//     OR32(ENABLE_IRQS_2, 1 << (49 - 32));
//     dev_barrier();
//     OR32(gpio_fen0, 1 << (pin % 32));
//     dev_barrier();
// }

// int gpio_event_detected(unsigned pin) {
//     dev_barrier();
//     if (pin > 32) 
//         gpio_panic("illegal pin=%d\n", pin);
//     int value = 0;
//     if (GET32(gpio_eds0) & (1 << (pin))) {
//         value = 1;
//     }
//     dev_barrier();
//     return value;
// }

// void gpio_event_clear(unsigned pin) {
//     dev_barrier();
//     if (pin > 32) 
//         gpio_panic("illegal pin=%d\n", pin);
//     PUT32(gpio_eds0, (1 << (pin)));
//     dev_barrier();
// }