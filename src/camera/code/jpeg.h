#ifndef __JPEG_H__
#define __JPEG_H__

#include "rpi.h"

// Encode an RGB888 image to baseline JPEG in memory
int jpeg_encode(const uint8_t *rgb, int width, int height,
                int quality, uint8_t *outbuf, int outbuf_size);

#endif
