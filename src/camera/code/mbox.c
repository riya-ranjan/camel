#include "rpi.h"
#include "mbox.h"

// dump out the entire messaage.  useful for debug.
void msg_dump(const char *msg, volatile uint32_t *u, unsigned nwords) {
    printk("%s\n", msg);
    for(int i = 0; i < nwords; i++)
        output("u[%d]=%x\n", i,u[i]);
}

/*
  This is given.

  Get board serial
    Tag: 0x00010004
    Request: Length: 0
    Response: Length: 8
    Value: u64: board serial
*/
uint64_t rpi_get_serialnum(void) {
    // 16-byte aligned 32-bit array
    volatile uint32_t msg[8] __attribute__((aligned(16)));

    // make sure aligned
    assert((unsigned)msg%16 == 0);

    msg[0] = 8*4;         // total size in bytes.
    msg[1] = 0;           // sender: always 0.
    msg[2] = 0x00010004;  // serial tag
    msg[3] = 8;           // total bytes avail for reply
    msg[4] = 0;           // request code [0].
    msg[5] = 0;           // space for 1st word of reply 
    msg[6] = 0;           // space for 2nd word of reply 
    msg[7] = 0;   // end tag

    // send and receive message
    mbox_send(MBOX_CH, msg);

#if 0
    // if you want to debug.
    output("got:\n");
    for(int i = 0; i < 8; i++)
        output("msg[%d]=%x\n", i, msg[i]);
#endif

    // should have value for success: 1<<31
    if(msg[1] != 0x80000000)
		panic("invalid response: got %x\n", msg[1]);

    // high bit should be set and reply size
    assert(msg[4] == ((1<<31) | 8));

    // for me the upper 32 bits were never non-zero.  
    // not sure if always true?
    assert(msg[6] == 0);
    return msg[5];
}

uint32_t rpi_get_memsize(void) {
    // 16-byte aligned 32-bit array
    volatile uint32_t msg[8] __attribute__((aligned(16)));

    // make sure aligned
    assert((unsigned)msg%16 == 0);

    msg[0] = 8*4;         // total size in bytes.
    msg[1] = 0;           // sender: always 0.
    msg[2] = 0x00010005;  // serial tag
    msg[3] = 8;           // total bytes avail for reply
    msg[4] = 0;           // request code [0].
    msg[5] = 0;           // space for 1st word of reply 
    msg[6] = 0;           // space for 2nd word of reply 
    msg[7] = 0;   // end tag

    // send and receive message
    mbox_send(MBOX_CH, msg);

#if 0
    // if you want to debug.
    output("got:\n");
    for(int i = 0; i < 8; i++)
        output("msg[%d]=%x\n", i, msg[i]);
#endif

    // should have value for success: 1<<31
    if(msg[1] != 0x80000000)
		panic("invalid response: got %x\n", msg[1]);

    // high bit should be set and reply size
    assert(msg[4] == ((1<<31) | 8));
    return msg[6];
}


uint32_t rpi_get_model(void) {
    // 16-byte aligned 32-bit array
    volatile uint32_t msg[8] __attribute__((aligned(16)));

    // make sure aligned
    assert((unsigned)msg%16 == 0);

    msg[0] = 8*4;         // total size in bytes.
    msg[1] = 0;           // sender: always 0.
    msg[2] = 0x00010001;  // serial tag
    msg[3] = 4;           // total bytes avail for reply
    msg[4] = 0;           // request code [0].
    msg[5] = 0;           // space for 1st word of reply 
    msg[6] = 0;           // space for 2nd word of reply 
    msg[7] = 0;   // end tag

    // send and receive message
    mbox_send(MBOX_CH, msg);

#if 0
    // if you want to debug.
    output("got:\n");
    for(int i = 0; i < 8; i++)
        output("msg[%d]=%x\n", i, msg[i]);
#endif

    // should have value for success: 1<<31
    if(msg[1] != 0x80000000)
		panic("invalid response: got %x\n", msg[1]);

    // high bit should be set and reply size
    assert(msg[4] == ((1<<31) | 4));
    return msg[5];
}

// https://www.raspberrypi-spy.co.uk/2012/09/checking-your-raspberry-pi-board-version/
uint32_t rpi_get_revision(void) {
    // 16-byte aligned 24-bit array
    volatile uint32_t msg[8] __attribute__((aligned(16)));

    // make sure aligned
    assert((unsigned)msg%16 == 0);

    msg[0] = 8*4;         // total size in bytes.
    msg[1] = 0;           // sender: always 0.
    msg[2] = 0x00010002;  // serial tag
    msg[3] = 4;           // total bytes avail for reply
    msg[4] = 0;           // request code [0].
    msg[5] = 0;           // space for 1st word of reply 
    msg[6] = 0;           // space for 2nd word of reply
    msg[7] = 0;             // end tag

    // send and receive message
    mbox_send(MBOX_CH, msg);

#if 0
    // if you want to debug.
    output("got:\n");
    for(int i = 0; i < 8; i++)
        output("msg[%d]=%x\n", i, msg[i]);
#endif

    // should have value for success: 1<<31
    if(msg[1] != 0x80000000)
		panic("invalid response: got %x\n", msg[1]);

    // high bit should be set and reply size
    assert(msg[4] == ((1<<31) | 4));
    return msg[5];
}

uint32_t rpi_temp_get(void) {
        // 16-byte aligned 32-bit array
    volatile uint32_t msg[8] __attribute__((aligned(16)));

    // make sure aligned
    assert((unsigned)msg%16 == 0);

    msg[0] = 8*4;         // total size in bytes.
    msg[1] = 0;           // sender: always 0.
    msg[2] = 0x00030006;  // serial tag
    msg[3] = 8;           // total bytes avail for reply
    msg[4] = 0;           // request code [0].
    msg[5] = 0;           // space for 1st word of reply 
    msg[6] = 0;           // space for 2nd word of reply 
    msg[7] = 0;   // end tag

    // send and receive message
    mbox_send(MBOX_CH, msg);

#if 0
    // if you want to debug.
    output("got:\n");
    for(int i = 0; i < 8; i++)
        output("msg[%d]=%x\n", i, msg[i]);
#endif

    // should have value for success: 1<<31
    if(msg[1] != 0x80000000)
		panic("invalid response: got %x\n", msg[1]);

    // high bit should be set and reply size
    assert(msg[4] == ((1<<31) | 8));
    assert(msg[5] == 0);
    return msg[6];
}

uint32_t rpi_clock_curhz_get(uint32_t clk) {
        // 16-byte aligned 32-bit array
    volatile uint32_t msg[8] __attribute__((aligned(16)));

    // make sure aligned
    assert((unsigned)msg%16 == 0);

    msg[0] = 8*4;         // total size in bytes.
    msg[1] = 0;           // sender: always 0.
    msg[2] = 0x00030002;  // serial tag
    msg[3] = 8;           // total bytes avail for reply
    msg[4] = 0;           // request code [0].
    msg[5] = clk;          
    msg[6] = 0;           // space for 2nd word of reply 
    msg[7] = 0;   // end tag

    // send and receive message
    mbox_send(MBOX_CH, msg);

#if 0
    // if you want to debug.
    output("got:\n");
    for(int i = 0; i < 8; i++)
        output("msg[%d]=%x\n", i, msg[i]);
#endif

    // should have value for success: 1<<31
    if(msg[1] != 0x80000000)
		panic("invalid response: got %x\n", msg[1]);

    // high bit should be set and reply size
    assert(msg[4] == ((1<<31) | 8));
    assert(msg[5] == clk);
    return msg[6];
}

uint32_t rpi_clock_maxhz_get(uint32_t clk) {
        // 16-byte aligned 32-bit array
    volatile uint32_t msg[8] __attribute__((aligned(16)));

    // make sure aligned
    assert((unsigned)msg%16 == 0);

    msg[0] = 8*4;         // total size in bytes.
    msg[1] = 0;           // sender: always 0.
    msg[2] = 0x00030004;  // serial tag
    msg[3] = 8;           // total bytes avail for reply
    msg[4] = 0;           // request code [0].
    msg[5] = clk;          
    msg[6] = 0;           // space for 2nd word of reply 
    msg[7] = 0;   // end tag

    // send and receive message
    mbox_send(MBOX_CH, msg);

#if 0
    // if you want to debug.
    output("got:\n");
    for(int i = 0; i < 8; i++)
        output("msg[%d]=%x\n", i, msg[i]);
#endif

    // should have value for success: 1<<31
    if(msg[1] != 0x80000000)
		panic("invalid response: got %x\n", msg[1]);

    // high bit should be set and reply size
    assert(msg[4] == ((1<<31) | 8));
    assert(msg[5] == clk);
    return msg[6];
}

uint32_t rpi_clock_realhz_get(uint32_t clk) {
        // 16-byte aligned 32-bit array
    volatile uint32_t msg[8] __attribute__((aligned(16)));

    // make sure aligned
    assert((unsigned)msg%16 == 0);

    msg[0] = 8*4;         // total size in bytes.
    msg[1] = 0;           // sender: always 0.
    msg[2] = 0x00030047;  // serial tag
    msg[3] = 8;           // total bytes avail for reply
    msg[4] = 0;           // request code [0].
    msg[5] = clk;          
    msg[6] = 0;           // space for 2nd word of reply 
    msg[7] = 0;   // end tag

    // send and receive message
    mbox_send(MBOX_CH, msg);

#if 0
    // if you want to debug.
    output("got:\n");
    for(int i = 0; i < 8; i++)
        output("msg[%d]=%x\n", i, msg[i]);
#endif

    // should have value for success: 1<<31
    if(msg[1] != 0x80000000)
		panic("invalid response: got %x\n", msg[1]);

    // high bit should be set and reply size
    assert(msg[4] == ((1<<31) | 8));
    assert(msg[5] == clk);
    return msg[6];
}

uint32_t rpi_clock_hz_set(uint32_t clk, uint32_t hz) {
        // 16-byte aligned 32-bit array
    volatile uint32_t msg[9] __attribute__((aligned(16)));

    // make sure aligned
    assert((unsigned)msg%16 == 0);

    msg[0] = 9*4;         // total size in bytes.
    msg[1] = 0;           // sender: always 0.
    msg[2] = 0x00038002;  // serial tag
    msg[3] = 12;           // total bytes avail for reply
    msg[4] = 0;           // request code [0].
    msg[5] = clk;
    msg[6] = hz;           // space for 2nd word of reply
    msg[7] = 1;
    msg[8] = 0;   // end tag

    // send and receive message
    mbox_send(MBOX_CH, msg);

#if 0
    // if you want to debug.
    output("got:\n");
    for(int i = 0; i < 8; i++)
        output("msg[%d]=%x\n", i, msg[i]);
#endif

    // should have value for success: 1<<31
    if(msg[1] != 0x80000000)
		panic("invalid response: got %x\n", msg[1]);

    // high bit should be set and reply size
    assert(msg[4] == ((1<<31) | 8));
    assert(msg[5] == clk);
    return msg[6];
}

uint32_t rpi_domain_enable(uint32_t domain) {
    volatile uint32_t msg[8] __attribute__((aligned(16)));
    msg[0] = 8 * 4;
    msg[1] = 0;
    msg[2] = 0x00038030;  // SET_DOMAIN_STATE
    msg[3] = 8;
    msg[4] = 0;
    msg[5] = domain;
    msg[6] = 1;           // ON
    msg[7] = 0;           // end tag

    mbox_send(MBOX_CH, msg);
    return msg[5];
}

// FUNCTIONS FOR UNICAM: ALLOCATE MEMORY ON THE GPU

uint32_t gpu_mem_alloc(uint32_t size, uint32_t alignment, uint32_t flags) {
    volatile uint32_t msg[9] __attribute__((aligned(16)));
    msg[0] = 9 * 4;
    msg[1] = 0;
    msg[2] = MBOX_TAG_GPU_MEM_ALLOC;
    msg[3] = 12;    // response buffer: 3 words
    msg[4] = 0;
    msg[5] = size;
    msg[6] = alignment;
    msg[7] = flags;
    msg[8] = 0;     // end tag

    mbox_send(MBOX_CH, msg);
    return msg[5];  // handle
}

uint32_t gpu_mem_lock(uint32_t handle) {
    volatile uint32_t msg[8] __attribute__((aligned(16)));
    msg[0] = 8 * 4;
    msg[1] = 0;
    msg[2] = MBOX_TAG_GPU_MEM_LOCK;
    msg[3] = 4;
    msg[4] = 0;
    msg[5] = handle;
    msg[6] = 0;
    msg[7] = 0;     // end tag

    mbox_send(MBOX_CH, msg);
    return msg[5];  // bus address
}

uint32_t gpu_mem_unlock(uint32_t handle) {
    volatile uint32_t msg[8] __attribute__((aligned(16)));
    msg[0] = 8 * 4;
    msg[1] = 0;
    msg[2] = MBOX_TAG_GPU_MEM_UNLOCK;
    msg[3] = 4;
    msg[4] = 0;
    msg[5] = handle;
    msg[6] = 0;
    msg[7] = 0;     // end tag

    mbox_send(MBOX_CH, msg);
    return msg[5];  // status (0 = success)
}

uint32_t gpu_mem_free(uint32_t handle) {
    volatile uint32_t msg[8] __attribute__((aligned(16)));
    msg[0] = 8 * 4;
    msg[1] = 0;
    msg[2] = MBOX_TAG_GPU_MEM_FREE;
    msg[3] = 4;
    msg[4] = 0;
    msg[5] = handle;
    msg[6] = 0;
    msg[7] = 0;     // end tag

    mbox_send(MBOX_CH, msg);
    return msg[5];  // status (0 = success)
}