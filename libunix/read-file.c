#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libunix.h"

// allocate buffer, read entire file into it, return it.   
// buffer is zero padded to a multiple of 4.
//
//  - <size> = exact nbytes of file.
//  - for allocation: round up allocated size to 4-byte multiple, pad
//    buffer with 0s. 
//
// fatal error: open/read of <name> fails.
//   - make sure to check all system calls for errors.
//   - make sure to close the file descriptor (this will
//     matter for later labs).
// 
void *read_file(unsigned *size, const char *name) {
    // How: 
    //    - use stat() to get the size of the file.
    //    - round up to a multiple of 4.
    //    - allocate a buffer
    //    - zero pads to a multiple of 4.
    //    - read entire file into buffer (read_exact())
    //    - fclose() the file descriptor
    //    - make sure any padding bytes have zeros.
    //    - return it.   
    struct stat st;
    int fd = open(name, O_RDONLY);
    if (fd == -1) {
        panic("open failed");
    }
    printf("Reading file: %s\n", name);
    if (stat(name, &st) != 0) {
        panic("stat failed");
    }
    unsigned file_size = st.st_size;
    unsigned alloc_size = pi_roundup(file_size, 4);
    void *buffer = calloc(1, alloc_size);
    if (buffer == NULL) {
        panic("calloc failed");
    }
    if (file_size > 0) {
        read_exact(fd, buffer, file_size);
    }
    close(fd);
    *size = file_size;
    return buffer;
}