#!/usr/bin/env python3
"""
Decode a captured RAW12 image from the Pi's UART output.

Usage:
    python3 decode_image.py capture.log
    python3 decode_image.py capture.log --color
    python3 decode_image.py capture.log --color --bayer BGGR
    python3 decode_image.py capture.log --packing mipi
    python3 decode_image.py capture.log --tryall   # try all combinations
"""
import sys
import argparse
import struct


def parse_raw12_from_log(filename):
    """Extract packed RAW12 hex data from UART log between markers."""
    with open(filename, 'r') as f:
        lines = f.readlines()

    img_start = img_end = None
    width = height = cx = cy = 0

    for i, line in enumerate(lines):
        if line.startswith('---RAW12 '):
            parts = line.strip().rstrip('-').split()
            width = int(parts[1])
            height = int(parts[2])
            cx = int(parts[3]) if len(parts) > 3 else 0
            cy = int(parts[4]) if len(parts) > 4 else 0
            img_start = i + 1
        elif line.strip() == '---END---' and img_start is not None:
            img_end = i
            break

    if img_start is None or img_end is None:
        print("ERROR: Could not find ---RAW12 ... ---END--- markers.")
        sys.exit(1)

    print(f"Found RAW12 crop: {width}x{height} at ({cx},{cy})")

    raw_lines = []
    for i in range(img_start, img_end):
        hexstr = lines[i].strip()
        if not hexstr:
            continue
        raw_lines.append(bytes.fromhex(hexstr))

    if len(raw_lines) != height:
        print(f"WARNING: Expected {height} lines, got {len(raw_lines)}")
        height = len(raw_lines)

    return raw_lines, width, height


def unpack_raw12(raw_lines, width, height, packing='mipi'):
    """
    Unpack packed RAW12 data to 12-bit pixel values.

    MIPI CSI-2 / V4L2 packed RAW12 format:
      byte0 = P0[11:4]  (upper 8 bits of pixel 0)
      byte1 = P1[11:4]  (upper 8 bits of pixel 1)
      byte2 = P1[3:0] << 4 | P0[3:0]  (lower 4 bits of both)
    """
    pixels = []
    for y in range(height):
        row = []
        raw = raw_lines[y]
        for x in range(0, width, 2):
            idx = (x // 2) * 3
            if idx + 2 >= len(raw):
                row.extend([0, 0])
                continue
            b0, b1, b2 = raw[idx], raw[idx+1], raw[idx+2]
            p0 = (b0 << 4) | (b2 & 0x0F)
            p1 = (b1 << 4) | (b2 >> 4)
            row.append(p0)
            row.append(p1)
        pixels.append(row[:width])
    return pixels


def to_8bit(pixels):
    return [[min(255, p >> 4) for p in row] for row in pixels]


def median_filter_raw(pixels, width, height):
    """Apply 3x3 median filter on same-color Bayer positions (stride 2)."""
    out = [row[:] for row in pixels]
    for y in range(2, height - 2):
        for x in range(2, width - 2):
            vals = []
            for dy in (-2, 0, 2):
                for dx in (-2, 0, 2):
                    vals.append(pixels[y+dy][x+dx])
            vals.sort()
            out[y][x] = vals[4]
    return out


def auto_white_balance(rgb, width, height):
    """Simple gray-world white balance on 8-bit RGB image."""
    r_sum = g_sum = b_sum = 0
    n = width * height
    for y in range(height):
        for x in range(width):
            r_sum += rgb[y][x][0]
            g_sum += rgb[y][x][1]
            b_sum += rgb[y][x][2]
    r_avg = r_sum / n if n else 1
    g_avg = g_sum / n if n else 1
    b_avg = b_sum / n if n else 1
    gray = (r_avg + g_avg + b_avg) / 3
    r_gain = gray / r_avg if r_avg > 0 else 1
    g_gain = gray / g_avg if g_avg > 0 else 1
    b_gain = gray / b_avg if b_avg > 0 else 1
    for y in range(height):
        for x in range(width):
            r, g, b = rgb[y][x]
            rgb[y][x] = [min(255, int(r * r_gain)),
                         min(255, int(g * g_gain)),
                         min(255, int(b * b_gain))]
    return rgb


def debayer(pixels, width, height, pattern='RGGB'):
    """
    Bilinear debayer. pattern is one of RGGB, BGGR, GRBG, GBRG.
    Returns list of rows, each row is list of [R, G, B] 8-bit values.
    """
    # Map pattern to channel at (0,0), (0,1), (1,0), (1,1).
    pat = {
        'RGGB': ('R','G','G','B'),
        'BGGR': ('B','G','G','R'),
        'GRBG': ('G','R','B','G'),
        'GBRG': ('G','B','R','G'),
    }[pattern]

    def ch(x, y):
        return pat[(y % 2) * 2 + (x % 2)]

    def px(x, y):
        if 0 <= x < width and 0 <= y < height:
            return pixels[y][x]
        return 0

    def avg(*vals):
        v = [x for x in vals if x > 0]
        return sum(v) // len(v) if v else 0

    rgb = []
    for y in range(height):
        row = []
        for x in range(width):
            c = ch(x, y)
            v = px(x, y)
            if c == 'R':
                r = v
                g = avg(px(x-1,y), px(x+1,y), px(x,y-1), px(x,y+1))
                b = avg(px(x-1,y-1), px(x+1,y-1), px(x-1,y+1), px(x+1,y+1))
            elif c == 'B':
                b = v
                g = avg(px(x-1,y), px(x+1,y), px(x,y-1), px(x,y+1))
                r = avg(px(x-1,y-1), px(x+1,y-1), px(x-1,y+1), px(x+1,y+1))
            elif c == 'G':
                g = v
                # Determine if R is on same row or same column.
                if ch(x-1, y) == 'R' or ch(x+1, y) == 'R':
                    r = avg(px(x-1,y), px(x+1,y))
                    b = avg(px(x,y-1), px(x,y+1))
                else:
                    b = avg(px(x-1,y), px(x+1,y))
                    r = avg(px(x,y-1), px(x,y+1))
            row.append([min(255, r >> 4), min(255, g >> 4), min(255, b >> 4)])
        rgb.append(row)
    return rgb


def analyze_raw(raw_lines, width, height):
    """Print diagnostic info about the raw data."""
    print(f"\n=== Raw Data Analysis ===")
    print(f"Image: {width}x{height}")
    expected_bytes = (width * 3) // 2
    print(f"Expected bytes/line: {expected_bytes}, actual: {len(raw_lines[0])}")

    # Show first 12 bytes of first 8 lines.
    print(f"\nFirst 12 bytes of lines 0-7:")
    for y in range(min(8, height)):
        raw = raw_lines[y]
        hexvals = ' '.join(f'{b:02x}' for b in raw[:12])
        print(f"  line {y}: {hexvals}")

    # Show last 16 bytes of first 2 lines (padding area).
    print(f"\nLast 16 bytes of lines 0-1 (padding check):")
    for y in range(min(2, height)):
        raw = raw_lines[y]
        hexvals = ' '.join(f'{b:02x}' for b in raw[-16:])
        print(f"  line {y} [{len(raw)-16}:{len(raw)}]: {hexvals}")

    # Unpack and show pixel values.
    print(f"\nLine 0, pixels 0-7:")
    raw = raw_lines[0]
    for x in range(0, 8, 2):
        idx = (x // 2) * 3
        b0, b1, b2 = raw[idx], raw[idx+1], raw[idx+2]
        p0 = (b0 << 4) | (b2 & 0x0F)
        p1 = (b1 << 4) | (b2 >> 4)
        print(f"  px[{x}]={p0:4d}(0x{p0:03X})  px[{x+1}]={p1:4d}(0x{p1:03X})  "
              f"[bytes: {b0:02x} {b1:02x} {b2:02x}]")

    if height > 2:
        print(f"Line 2, pixels 0-7:")
        raw = raw_lines[2]
        for x in range(0, 8, 2):
            idx = (x // 2) * 3
            b0, b1, b2 = raw[idx], raw[idx+1], raw[idx+2]
            p0 = (b0 << 4) | (b2 & 0x0F)
            p1 = (b1 << 4) | (b2 >> 4)
            print(f"  px[{x}]={p0:4d}(0x{p0:03X})  px[{x+1}]={p1:4d}(0x{p1:03X})  "
                  f"[bytes: {b0:02x} {b1:02x} {b2:02x}]")

    # Show samples across the width at line 0.
    print(f"Line 0, samples across width:")
    raw = raw_lines[0]
    bar_w = width // 8
    for bar in range(8):
        x = bar * bar_w
        if x % 2 == 1:
            x -= 1
        idx = (x // 2) * 3
        if idx + 2 >= len(raw):
            break
        b0, b1, b2 = raw[idx], raw[idx+1], raw[idx+2]
        p0 = (b0 << 4) | (b2 & 0x0F)
        p1 = (b1 << 4) | (b2 >> 4)
        print(f"  bar{bar} px[{x}]={p0:4d}(0x{p0:03X})  px[{x+1}]={p1:4d}(0x{p1:03X})")


def save_image(filename, data, width, height, color=False):
    try:
        from PIL import Image
        if color:
            img = Image.new('RGB', (width, height))
            for y in range(height):
                for x in range(width):
                    img.putpixel((x, y), tuple(data[y][x]))
        else:
            img = Image.new('L', (width, height))
            for y in range(height):
                for x in range(width):
                    img.putpixel((x, y), data[y][x])
        if not filename.endswith('.png'):
            filename = filename.rsplit('.', 1)[0] + '.png'
        img.save(filename)
        print(f"Saved: {filename}")
        return filename
    except ImportError:
        ext = '.ppm' if color else '.pgm'
        filename = filename.rsplit('.', 1)[0] + ext
        with open(filename, 'wb') as f:
            if color:
                f.write(f"P6\n{width} {height}\n255\n".encode())
                for row in data:
                    for r, g, b in row:
                        f.write(bytes([min(255,max(0,r)), min(255,max(0,g)), min(255,max(0,b))]))
            else:
                f.write(f"P5\n{width} {height}\n255\n".encode())
                for row in data:
                    f.write(bytes(min(255, max(0, v)) for v in row))
        print(f"Saved: {filename}")
        return filename


def main():
    parser = argparse.ArgumentParser(
        description='Decode RAW12 image from Pi UART capture log')
    parser.add_argument('logfile', help='UART capture log file')
    parser.add_argument('-o', '--output', default='captured.png')
    parser.add_argument('--color', action='store_true',
        help='Apply Bayer debayering for color output')
    parser.add_argument('--packing', default='mipi',
        help='RAW12 byte packing format (ignored, kept for compat)')
    parser.add_argument('--bayer', choices=['RGGB','BGGR','GRBG','GBRG'],
        default='RGGB', help='Bayer pattern (default: RGGB)')
    parser.add_argument('--analyze', action='store_true',
        help='Print raw data analysis')
    parser.add_argument('--tryall', action='store_true',
        help='Try all packing+bayer combinations')
    parser.add_argument('--gamma', type=float, default=1.0)
    parser.add_argument('--denoise', action='store_true',
        help='Apply median filter before debayering')
    parser.add_argument('--awb', action='store_true',
        help='Apply gray-world auto white balance')
    args = parser.parse_args()

    raw_lines, width, height = parse_raw12_from_log(args.logfile)

    if args.analyze or args.tryall:
        analyze_raw(raw_lines, width, height)

    if args.tryall:
        # Generate images for all combinations.
        for packing in ['mipi', 'v4l2']:
            pixels = unpack_raw12(raw_lines, width, height, packing)
            # Grayscale for each packing.
            gray = to_8bit(pixels)
            name = f"try_{packing}_gray.png"
            save_image(name, gray, width, height, color=False)
            # Color for each bayer pattern.
            for bayer in ['RGGB', 'BGGR', 'GRBG', 'GBRG']:
                rgb = debayer(pixels, width, height, bayer)
                name = f"try_{packing}_{bayer}.png"
                save_image(name, rgb, width, height, color=True)
        print("\nGenerated all combinations. Look at the images to find the correct one.")
        return

    # Normal decode.
    pixels = unpack_raw12(raw_lines, width, height, args.packing)

    if args.denoise:
        print("Applying median filter...")
        pixels = median_filter_raw(pixels, width, height)

    if args.color:
        print(f"Debayering ({args.bayer}, {args.packing} packing)...")
        rgb = debayer(pixels, width, height, args.bayer)
        if args.awb:
            print("Applying auto white balance...")
            rgb = auto_white_balance(rgb, width, height)
        if args.gamma != 1.0:
            for y in range(height):
                for x in range(width):
                    rgb[y][x] = [int(255*(c/255)**(1/args.gamma)) for c in rgb[y][x]]
        save_image(args.output, rgb, width, height, color=True)
    else:
        gray = to_8bit(pixels)
        if args.gamma != 1.0:
            for y in range(height):
                for x in range(width):
                    gray[y][x] = int(255*(gray[y][x]/255)**(1/args.gamma))
        save_image(args.output, gray, width, height, color=False)


if __name__ == '__main__':
    main()
