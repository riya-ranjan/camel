// BARE METAL JPEG LIBRARY
// STRIPPED DOWN VERSION OF JPEG CLUB LIBRARY
// https://jpegclub.org/cjpeg/jfdctint.c

#include "rpi.h"
#include "jpeg.h"

// ----------------------------------------------------------------
// Zig-zag scan order
// ----------------------------------------------------------------
static const uint8_t zz[64] = {
     0, 1, 8,16, 9, 2, 3,10,  17,24,32,25,18,11, 4, 5,
    12,19,26,33,40,48,41,34,  27,20,13, 6, 7,14,21,28,
    35,42,49,56,57,50,43,36,  29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,  53,60,61,54,47,55,62,63
};

// ----------------------------------------------------------------
// Standard quantization tables (Annex K, Tables K.1 & K.2)
// Stored in zig-zag order for convenience during encoding.
// ----------------------------------------------------------------
static const uint8_t std_lum_qt[64] = {
    16,11,10,16,24,40,51,61, 12,12,14,19,26,58,60,55,
    14,13,16,24,40,57,69,56, 14,17,22,29,51,87,80,62,
    18,22,37,56,68,109,103,77, 24,35,55,64,81,104,113,92,
    49,64,78,87,103,121,120,101, 72,92,95,98,112,100,103,99
};

static const uint8_t std_chrom_qt[64] = {
    17,18,24,47,99,99,99,99, 18,21,26,66,99,99,99,99,
    24,26,56,99,99,99,99,99, 47,66,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99, 99,99,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99, 99,99,99,99,99,99,99,99
};

// ----------------------------------------------------------------
// Standard Huffman tables (Annex K, Tables K.3–K.6)
// ----------------------------------------------------------------

// DC luminance
static const uint8_t dc_lum_bits[16] =
    {0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
static const uint8_t dc_lum_vals[12] =
    {0,1,2,3,4,5,6,7,8,9,10,11};

// DC chrominance
static const uint8_t dc_chr_bits[16] =
    {0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
static const uint8_t dc_chr_vals[12] =
    {0,1,2,3,4,5,6,7,8,9,10,11};

// AC luminance
static const uint8_t ac_lum_bits[16] =
    {0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d};
static const uint8_t ac_lum_vals[162] = {
    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,
    0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
    0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,
    0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
    0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,
    0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
    0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,
    0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
    0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,
    0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,
    0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
    0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,
    0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
    0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,
    0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
    0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,
    0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

// AC chrominance
static const uint8_t ac_chr_bits[16] =
    {0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77};
static const uint8_t ac_chr_vals[162] = {
    0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,
    0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
    0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,
    0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,
    0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,
    0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
    0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,
    0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,
    0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,
    0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
    0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,
    0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,
    0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
    0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,
    0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
    0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,
    0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
    0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,
    0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

// ----------------------------------------------------------------
// Huffman encode table: symbol -> (code, bit length)
// ----------------------------------------------------------------
typedef struct { uint16_t code; uint8_t len; } huff_sym_t;
typedef struct { huff_sym_t e[256]; } huff_t;

static huff_t ht_dc_lum, ht_dc_chr, ht_ac_lum, ht_ac_chr;

static void build_huff(huff_t *ht, const uint8_t *bits, const uint8_t *vals, int nvals) {
    memset(ht, 0, sizeof(*ht));
    uint32_t code = 0;
    int vi = 0;
    for (int len = 1; len <= 16; len++) {
        for (int i = 0; i < bits[len - 1] && vi < nvals; i++, vi++) {
            ht->e[vals[vi]].code = code;
            ht->e[vals[vi]].len  = len;
            code++;
        }
        code <<= 1;
    }
}

static int huff_tables_built = 0;

static void build_all_huff(void) {
    if (huff_tables_built) return;
    build_huff(&ht_dc_lum, dc_lum_bits, dc_lum_vals, 12);
    build_huff(&ht_dc_chr, dc_chr_bits, dc_chr_vals, 12);
    build_huff(&ht_ac_lum, ac_lum_bits, ac_lum_vals, 162);
    build_huff(&ht_ac_chr, ac_chr_bits, ac_chr_vals, 162);
    huff_tables_built = 1;
}

// ----------------------------------------------------------------
// Bit-stream writer with JPEG byte stuffing
// ----------------------------------------------------------------
typedef struct {
    uint8_t *buf;
    int cap, pos;
    uint32_t bits;
    int nbits;
    int overflow;
} bw_t;

static void bw_init(bw_t *w, uint8_t *buf, int cap) {
    w->buf = buf; w->cap = cap; w->pos = 0;
    w->bits = 0; w->nbits = 0; w->overflow = 0;
}

static inline void bw_emit_byte(bw_t *w, uint8_t b) {
    if (w->pos < w->cap) w->buf[w->pos++] = b;
    else w->overflow = 1;
}

static void bw_putbits(bw_t *w, uint16_t code, int len) {
    w->bits = (w->bits << len) | (code & ((1 << len) - 1));
    w->nbits += len;
    while (w->nbits >= 8) {
        w->nbits -= 8;
        uint8_t byte = (w->bits >> w->nbits) & 0xFF;
        bw_emit_byte(w, byte);
        if (byte == 0xFF)
            bw_emit_byte(w, 0x00);  // byte stuffing
    }
}

static void bw_flush(bw_t *w) {
    if (w->nbits > 0) {
        int pad = 8 - w->nbits;
        uint8_t byte = ((w->bits << pad) | ((1 << pad) - 1)) & 0xFF;
        bw_emit_byte(w, byte);
        if (byte == 0xFF)
            bw_emit_byte(w, 0x00);
        w->nbits = 0;
        w->bits = 0;
    }
}

static void bw_byte(bw_t *w, uint8_t b) { bw_emit_byte(w, b); }

static void bw_word(bw_t *w, uint16_t v) {
    bw_emit_byte(w, v >> 8);
    bw_emit_byte(w, v & 0xFF);
}

// ----------------------------------------------------------------
// Integer forward DCT (Loeffler/Ligtenberg/Moschytz algorithm)
// Based on IJG jfdctint.c — public domain.
// ----------------------------------------------------------------
#define CONST_BITS  13
#define PASS1_BITS  2
#define DESCALE(x,n) (((x) + (1 << ((n)-1))) >> (n))

#define FIX_0_298  2446
#define FIX_0_390  3196
#define FIX_0_541  4433
#define FIX_0_765  6270
#define FIX_0_899  7373
#define FIX_1_175  9633
#define FIX_1_501  12299
#define FIX_1_847  15137
#define FIX_1_961  16069
#define FIX_2_053  16819
#define FIX_2_562  20995
#define FIX_3_072  25172

static void fdct(int *d) {
    int *dp;
    int z1, z2, z3, z4, z5;

    // Pass 1: rows
    dp = d;
    for (int i = 0; i < 8; i++, dp += 8) {
        int t0 = dp[0] + dp[7];
        int t7 = dp[0] - dp[7];
        int t1 = dp[1] + dp[6];
        int t6 = dp[1] - dp[6];
        int t2 = dp[2] + dp[5];
        int t5 = dp[2] - dp[5];
        int t3 = dp[3] + dp[4];
        int t4 = dp[3] - dp[4];

        int e0 = t0 + t3;
        int e3 = t0 - t3;
        int e1 = t1 + t2;
        int e2 = t1 - t2;

        dp[0] = (e0 + e1) << PASS1_BITS;
        dp[4] = (e0 - e1) << PASS1_BITS;

        z1 = (e2 + e3) * FIX_0_541;
        dp[2] = DESCALE(z1 + e3 * FIX_0_765, CONST_BITS - PASS1_BITS);
        dp[6] = DESCALE(z1 - e2 * FIX_1_847, CONST_BITS - PASS1_BITS);

        z1 = t4 + t7;
        z2 = t5 + t6;
        z3 = t4 + t6;
        z4 = t5 + t7;
        z5 = (z3 + z4) * FIX_1_175;

        t4 = t4 * FIX_0_298;
        t5 = t5 * FIX_2_053;
        t6 = t6 * FIX_3_072;
        t7 = t7 * FIX_1_501;
        z1 = z1 * (-FIX_0_899);
        z2 = z2 * (-FIX_2_562);
        z3 = z3 * (-FIX_1_961);
        z4 = z4 * (-FIX_0_390);

        z3 += z5;
        z4 += z5;

        dp[7] = DESCALE(t4 + z1 + z3, CONST_BITS - PASS1_BITS);
        dp[5] = DESCALE(t5 + z2 + z4, CONST_BITS - PASS1_BITS);
        dp[3] = DESCALE(t6 + z2 + z3, CONST_BITS - PASS1_BITS);
        dp[1] = DESCALE(t7 + z1 + z4, CONST_BITS - PASS1_BITS);
    }

    // Pass 2: columns
    dp = d;
    for (int i = 0; i < 8; i++, dp++) {
        int t0 = dp[0*8] + dp[7*8];
        int t7 = dp[0*8] - dp[7*8];
        int t1 = dp[1*8] + dp[6*8];
        int t6 = dp[1*8] - dp[6*8];
        int t2 = dp[2*8] + dp[5*8];
        int t5 = dp[2*8] - dp[5*8];
        int t3 = dp[3*8] + dp[4*8];
        int t4 = dp[3*8] - dp[4*8];

        int e0 = t0 + t3;
        int e3 = t0 - t3;
        int e1 = t1 + t2;
        int e2 = t1 - t2;

        dp[0*8] = DESCALE(e0 + e1, PASS1_BITS);
        dp[4*8] = DESCALE(e0 - e1, PASS1_BITS);

        z1 = (e2 + e3) * FIX_0_541;
        dp[2*8] = DESCALE(z1 + e3 * FIX_0_765, CONST_BITS + PASS1_BITS);
        dp[6*8] = DESCALE(z1 - e2 * FIX_1_847, CONST_BITS + PASS1_BITS);

        z1 = t4 + t7;
        z2 = t5 + t6;
        z3 = t4 + t6;
        z4 = t5 + t7;
        z5 = (z3 + z4) * FIX_1_175;

        t4 = t4 * FIX_0_298;
        t5 = t5 * FIX_2_053;
        t6 = t6 * FIX_3_072;
        t7 = t7 * FIX_1_501;
        z1 = z1 * (-FIX_0_899);
        z2 = z2 * (-FIX_2_562);
        z3 = z3 * (-FIX_1_961);
        z4 = z4 * (-FIX_0_390);

        z3 += z5;
        z4 += z5;

        dp[7*8] = DESCALE(t4 + z1 + z3, CONST_BITS + PASS1_BITS);
        dp[5*8] = DESCALE(t5 + z2 + z4, CONST_BITS + PASS1_BITS);
        dp[3*8] = DESCALE(t6 + z2 + z3, CONST_BITS + PASS1_BITS);
        dp[1*8] = DESCALE(t7 + z1 + z4, CONST_BITS + PASS1_BITS);
    }
}

// ----------------------------------------------------------------
// Scale quantization table by quality factor (IJG formula)
// ----------------------------------------------------------------
static void scale_qt(const uint8_t *base, uint8_t *out, int quality) {
    int s = (quality < 50) ? (5000 / quality) : (200 - quality * 2);
    for (int i = 0; i < 64; i++) {
        int v = ((int)base[i] * s + 50) / 100;
        if (v < 1)   v = 1;
        if (v > 255) v = 255;
        out[i] = v;
    }
}

// ----------------------------------------------------------------
// Encode one 8x8 block: quantize, zigzag, Huffman encode
// Returns the new DC prediction value.
// ----------------------------------------------------------------
static int category(int v) {
    if (v < 0) v = -v;
    int cat = 0;
    while (v) { cat++; v >>= 1; }
    return cat;
}

static int encode_block(bw_t *w, int *block, const uint8_t *qt,
                        int prev_dc, const huff_t *dc_ht,
                        const huff_t *ac_ht)
{
    // Quantize + zigzag reorder
    int quant[64];
    for (int i = 0; i < 64; i++) {
        int val = block[zz[i]];
        // Round to nearest: (val + qt/2) / qt for positive, etc.
        if (val >= 0)
            quant[i] = (val + (qt[zz[i]] >> 1)) / qt[zz[i]];
        else
            quant[i] = (val - (qt[zz[i]] >> 1)) / qt[zz[i]];
    }

    // DC coefficient (differential)
    int dc = quant[0] - prev_dc;
    int cat_dc = category(dc);
    bw_putbits(w, dc_ht->e[cat_dc].code, dc_ht->e[cat_dc].len);
    if (cat_dc > 0) {
        int bits = dc;
        if (dc < 0) bits = dc - 1;  // 1's complement for negatives
        bw_putbits(w, bits & ((1 << cat_dc) - 1), cat_dc);
    }

    // AC coefficients
    int zero_run = 0;
    for (int i = 1; i < 64; i++) {
        if (quant[i] == 0) {
            zero_run++;
            continue;
        }
        // Emit ZRL (16 zeros) symbols as needed
        while (zero_run >= 16) {
            bw_putbits(w, ac_ht->e[0xF0].code, ac_ht->e[0xF0].len);
            zero_run -= 16;
        }
        int cat_ac = category(quant[i]);
        int sym = (zero_run << 4) | cat_ac;
        bw_putbits(w, ac_ht->e[sym].code, ac_ht->e[sym].len);
        int bits = quant[i];
        if (bits < 0) bits = quant[i] - 1;
        bw_putbits(w, bits & ((1 << cat_ac) - 1), cat_ac);
        zero_run = 0;
    }

    // EOB if there were trailing zeros
    if (zero_run > 0)
        bw_putbits(w, ac_ht->e[0x00].code, ac_ht->e[0x00].len);

    return quant[0];  // new DC prediction = this block's quantized DC
}

// ----------------------------------------------------------------
// JFIF header writing helpers
// ----------------------------------------------------------------

// Write DQT segment for one table
static void write_dqt(bw_t *w, int table_id, const uint8_t *qt) {
    bw_word(w, 0xFFDB);            // DQT marker
    bw_word(w, 67);                // length: 2 + 1 + 64
    bw_byte(w, table_id);          // 8-bit precision (0), table id
    // Write in zig-zag order
    for (int i = 0; i < 64; i++)
        bw_byte(w, qt[zz[i]]);
}

// Write DHT segment for one Huffman table
static void write_dht(bw_t *w, int class_id, const uint8_t *bits,
                       const uint8_t *vals, int nvals)
{
    bw_word(w, 0xFFC4);            // DHT marker
    bw_word(w, 2 + 1 + 16 + nvals);
    bw_byte(w, class_id);          // table class (0=DC,1=AC) | table id
    for (int i = 0; i < 16; i++)
        bw_byte(w, bits[i]);
    for (int i = 0; i < nvals; i++)
        bw_byte(w, vals[i]);
}

// ----------------------------------------------------------------
// Public API
// ----------------------------------------------------------------
int jpeg_encode(const uint8_t *rgb, int width, int height,
                int quality, uint8_t *outbuf, int outbuf_size)
{
    if (!rgb || width <= 0 || height <= 0 || !outbuf || outbuf_size < 1024)
        return 0;
    if (quality < 1)   quality = 1;
    if (quality > 100) quality = 100;

    build_all_huff();

    // Scale quantization tables
    uint8_t lum_qt[64], chr_qt[64];
    scale_qt(std_lum_qt, lum_qt, quality);
    scale_qt(std_chrom_qt, chr_qt, quality);

    bw_t w;
    bw_init(&w, outbuf, outbuf_size);

    // SOI
    bw_word(&w, 0xFFD8);

    // APP0 (JFIF)
    bw_word(&w, 0xFFE0);
    bw_word(&w, 16);               // length
    bw_byte(&w, 'J'); bw_byte(&w, 'F'); bw_byte(&w, 'I');
    bw_byte(&w, 'F'); bw_byte(&w, 0);
    bw_byte(&w, 1); bw_byte(&w, 1); // version 1.1
    bw_byte(&w, 0);                // no aspect ratio
    bw_word(&w, 1); bw_word(&w, 1); // pixel aspect 1:1
    bw_byte(&w, 0); bw_byte(&w, 0); // no thumbnail

    // DQT — luminance (table 0) and chrominance (table 1)
    write_dqt(&w, 0, lum_qt);
    write_dqt(&w, 1, chr_qt);

    // SOF0 — baseline DCT, YCbCr 4:4:4
    bw_word(&w, 0xFFC0);
    bw_word(&w, 8 + 3 * 3);        // length
    bw_byte(&w, 8);                // 8-bit precision
    bw_word(&w, height);
    bw_word(&w, width);
    bw_byte(&w, 3);                // 3 components
    // Y:  id=1, sampling=1x1 (0x11), quant table 0
    bw_byte(&w, 1); bw_byte(&w, 0x11); bw_byte(&w, 0);
    // Cb: id=2, sampling=1x1 (0x11), quant table 1
    bw_byte(&w, 2); bw_byte(&w, 0x11); bw_byte(&w, 1);
    // Cr: id=3, sampling=1x1 (0x11), quant table 1
    bw_byte(&w, 3); bw_byte(&w, 0x11); bw_byte(&w, 1);

    // DHT — 4 Huffman tables
    write_dht(&w, 0x00, dc_lum_bits, dc_lum_vals, 12);   // DC lum
    write_dht(&w, 0x10, ac_lum_bits, ac_lum_vals, 162);   // AC lum
    write_dht(&w, 0x01, dc_chr_bits, dc_chr_vals, 12);    // DC chr
    write_dht(&w, 0x11, ac_chr_bits, ac_chr_vals, 162);   // AC chr

    // SOS — start of scan
    bw_word(&w, 0xFFDA);
    bw_word(&w, 6 + 2 * 3);        // length
    bw_byte(&w, 3);                // 3 components
    bw_byte(&w, 1); bw_byte(&w, 0x00);  // Y:  DC table 0, AC table 0
    bw_byte(&w, 2); bw_byte(&w, 0x11);  // Cb: DC table 1, AC table 1
    bw_byte(&w, 3); bw_byte(&w, 0x11);  // Cr: DC table 1, AC table 1
    bw_byte(&w, 0);  // Ss
    bw_byte(&w, 63); // Se
    bw_byte(&w, 0);  // Ah=0, Al=0

    // ---- Encode scan data ----
    int prev_dc_y = 0, prev_dc_cb = 0, prev_dc_cr = 0;

    int blocks_w = (width + 7) / 8;
    int blocks_h = (height + 7) / 8;

    for (int by = 0; by < blocks_h; by++) {
        for (int bx = 0; bx < blocks_w; bx++) {
            // Process Y, Cb, Cr blocks
            int yb[64], cbb[64], crb[64];

            for (int j = 0; j < 8; j++) {
                for (int i = 0; i < 8; i++) {
                    int px = bx * 8 + i;
                    int py = by * 8 + j;
                    // Clamp to image bounds (replicate edge)
                    if (px >= width)  px = width - 1;
                    if (py >= height) py = height - 1;

                    int idx = (py * width + px) * 3;
                    int r = rgb[idx];
                    int g = rgb[idx + 1];
                    int b = rgb[idx + 2];

                    // RGB -> YCbCr (fixed-point, Q16)
                    //  Y  =  0.299R + 0.587G + 0.114B
                    //  Cb = -0.1687R - 0.3313G + 0.5B + 128
                    //  Cr =  0.5R - 0.4187G - 0.0813B + 128
                    int y  = ( 19595*r + 38470*g +  7471*b + 32768) >> 16;
                    int cb = (-11056*r - 21712*g + 32768*b + 32768) >> 16;
                    int cr = ( 32768*r - 27440*g -  5328*b + 32768) >> 16;

                    // Level shift: JPEG DCT expects -128..127
                    yb [j * 8 + i] = y  - 128;
                    cbb[j * 8 + i] = cb; // already centered (Cb/Cr +128 built into formula)
                    crb[j * 8 + i] = cr;
                }
            }

            // Forward DCT
            fdct(yb);
            fdct(cbb);
            fdct(crb);

            // Encode
            prev_dc_y  = encode_block(&w, yb,  lum_qt, prev_dc_y,
                                      &ht_dc_lum, &ht_ac_lum);
            prev_dc_cb = encode_block(&w, cbb, chr_qt, prev_dc_cb,
                                      &ht_dc_chr, &ht_ac_chr);
            prev_dc_cr = encode_block(&w, crb, chr_qt, prev_dc_cr,
                                      &ht_dc_chr, &ht_ac_chr);
        }
    }

    bw_flush(&w);

    // EOI
    bw_emit_byte(&w, 0xFF);
    bw_emit_byte(&w, 0xD9);

    if (w.overflow) return 0;
    return w.pos;
}
