// engler, cs140e: your code to find the tty-usb device on your laptop.
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include "libunix.h"
#include <stdbool.h>
#include <limits.h>

#define _SVID_SOURCE
#include <dirent.h>
static const char *ttyusb_prefixes[] = {
    "ttyUSB",	// linux
    "ttyACM",   // linux
    "cu.SLAB_USB", // mac os
    "cu.usbserial", // mac os
	0
};

static int filter(const struct dirent *d) {
    // scan through the prefixes, returning 1 when you find a match.
    // 0 if there is no match.
    for (int i = 0; ttyusb_prefixes[i] != 0; i++) {
        if (strncmp(d->d_name, ttyusb_prefixes[i], strlen(ttyusb_prefixes[i])) == 0) {
            return 1;
        }
    }
    return 0;
}

// find the TTY-usb device (if any) by using <scandir> to search for
// a device with a prefix given by <ttyusb_prefixes> in /dev
// returns:
//  - device name.
// error: panic's if 0 or more than 1 devices.
char *find_ttyusb(void) {
    // use <alphasort> in <scandir>
    // return a malloc'd name so doesn't corrupt.
    struct dirent **namelist;
    int n = scandir("/dev", &namelist, filter, alphasort);
    if (n < 0) {
        panic("scandir failed");
    } else if (n == 0) {
        panic("no ttyusb devices found");
    } else if (n > 1) {
        panic("more than one ttyusb device found");
    }
    char path[256];
    snprintf(path, sizeof path, "/dev/%s", namelist[0]->d_name);
    char *device_name = strdupf("%s", path);
    if (!device_name)
        panic("malloc failed");
    for (int i = 0; i < n; i++)
        free(namelist[i]);
    free(namelist);
    return device_name;
}

// return the most recently mounted ttyusb (the one
// mounted last).  use the modification time 
// returned by state.
char *find_ttyusb_last(void) {
    struct dirent **namelist;
    struct stat st;
    int n = scandir("/dev", &namelist, filter, alphasort);
    if (n < 0)
        panic("scandir failed");
    if (n == 0)
        panic("no ttyusb devices found");
    time_t newest = 0;
    char *device_name = NULL;
    for (int i = 0; i < n; i++) {
        char path[256];
        snprintf(path, sizeof path, "/dev/%s", namelist[i]->d_name);
        if (stat(path, &st) < 0)
            panic("stat failed");
        if (st.st_mtime > newest) {
            newest = st.st_mtime;
            free(device_name);
            device_name = strdupf("%s", path);
        }
        free(namelist[i]);
    }
    free(namelist);
    return device_name;
}

// return the oldest mounted ttyusb (the one mounted
// "first") --- use the modification returned by
// stat()
char *find_ttyusb_first(void) {
    struct dirent **namelist;
    struct stat st;
    int n = scandir("/dev", &namelist, filter, alphasort);
    if (n < 0)
        panic("scandir failed");
    if (n == 0)
        panic("no ttyusb devices found");
    time_t oldest = LONG_MAX;
    char *device_name = NULL;
    for (int i = 0; i < n; i++) {
        char path[256];
        snprintf(path, sizeof path, "/dev/%s", namelist[i]->d_name);
        if (stat(path, &st) < 0)
            panic("stat failed");
        if (st.st_mtime < oldest) {
            oldest = st.st_mtime;
            free(device_name);
            device_name = strdupf("%s", path);
        }
        free(namelist[i]);
    }
    free(namelist);
    return device_name;
}
