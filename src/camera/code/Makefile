# PROGS := tests/5-test-fat32.c
PROGS := tests/4-frame-capture.c
# PROGS := tests/3-sensor-init.c
# PROGS := tests/2-unicam-power.c
# PROGS := tests/1-sensor-id.c
# PROGS := tests/0-test-boot.c
# PROGS := tests/test-mbox.c
F = ../../fat32
COMMON_SRC := mbox.c cam-power.c i2c.c imx477.c unicam.c \
              $(F)/pi-sd.c $(F)/mbr.c $(F)/mbr-helpers.c \
              $(F)/fat32.c $(F)/fat32-helpers.c $(F)/fat32-lfn-helpers.c \
              $(F)/unicode-utf8.c $(F)/emmc.c
CFLAGS_EXTRA = -I$(F)

# define this if you need to give the device for your pi
TTYUSB =
BOOTLOADER = my-install
RUN=1

# if you need pl011
L = $(CS140E_2026_PATH)/../camel/libpi/staff-objs
STAFF_OBJS += $(L)/pl011-uart.o
STAFF_OBJS += $(L)/staff-kmalloc.o

# pull in libgcc in case you need division.
LIBGCC  = $(shell $(CC) -print-libgcc-file-name)
LIB_POST += $(LIBGCC)

include $(CS140E_2026_PATH)/../camel/libpi/mk/Makefile.robust-v2
