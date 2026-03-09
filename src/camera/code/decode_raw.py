#!/usr/bin/env python3
"""
Decode a binary RAW12 frame file (FRAME.RAW) from the IMX477 sensor.

Proper ISP pipeline:
  1. Unpack MIPI RAW12 -> 12-bit pixels
  2. Black level subtraction (IMX477 black level = 256)
  3. Normalize to float [0, 1]
  4. White balance (applied BEFORE demosaic to avoid color artifacts)
  5. Demosaic (OpenCV or bilinear fallback)
  6. Color correction matrix (from libcamera imx477.json)
  7. sRGB gamma correction
  8. Save as 8-bit PNG

Usage:
    python3 decode_raw.py FRAME.RAW
    python3 decode_raw.py FRAME.RAW --no-ccm
    python3 decode_raw.py FRAME.RAW --ct 2850
"""
import sys
import os
import argparse
import numpy as np

# IMX477 black level (12-bit). Confirmed by libcamera tuning + DNG metadata.
BLACK_LEVEL = 256
WHITE_LEVEL = 4095
USABLE_RANGE = WHITE_LEVEL - BLACK_LEVEL  # 3839

# IMX477 effective Bayer pattern in our 2x2 binned mode: BGGR
# Despite register 0x0101=0x00 (RGGB for full-res), the 2x2 binning
# phase shifts the pattern. Confirmed visually: BGGR gives natural
# skin tones while RGGB gives blue/purple skin.
DEFAULT_BAYER = 'BGGR'

# Calibrated AWB gains from libcamera vc4/data/imx477.json ct_curve.
# ct_curve values are [CT, Cr=G/R, Cb=G/B]; gains = 1/Cr, 1/Cb.
# These account for the IMX477's spectral response.
WB_GAIN_TABLE = {
    2360: {'R': 1.664, 'G': 1.0, 'B': 3.233},
    2848: {'R': 1.972, 'G': 1.0, 'B': 2.500},
    2903: {'R': 2.039, 'G': 1.0, 'B': 2.277},
    3628: {'R': 2.347, 'G': 1.0, 'B': 1.797},
    4660: {'R': 2.833, 'G': 1.0, 'B': 1.471},
    5579: {'R': 3.099, 'G': 1.0, 'B': 1.429},
    6125: {'R': 3.196, 'G': 1.0, 'B': 1.408},
    7763: {'R': 3.390, 'G': 1.0, 'B': 1.351},
    9505: {'R': 3.962, 'G': 1.0, 'B': 1.273},
}
DEFAULT_WB_GAINS = WB_GAIN_TABLE[5579]  # daylight default

# Color correction matrices from libcamera imx477.json.
# These transform white-balanced linear Bayer RGB -> sRGB linear.
CCM_TABLE = {
    2850: np.array([
        [ 1.97469, -0.71439, -0.26031],
        [-0.43521,  2.09769, -0.66248],
        [-0.04826, -0.84642,  1.89468],
    ]),
    3580: np.array([
        [ 2.03422, -0.80048, -0.23374],
        [-0.39089,  1.97221, -0.58132],
        [-0.08969, -0.61439,  1.70408],
    ]),
    4559: np.array([
        [ 2.15423, -0.98143, -0.17279],
        [-0.38131,  2.14763, -0.76632],
        [-0.10069, -0.54383,  1.64452],
    ]),
    5881: np.array([
        [ 2.18464, -0.95493, -0.22971],
        [-0.36826,  2.00298, -0.63471],
        [-0.15219, -0.38055,  1.53274],
    ]),
    7600: np.array([
        [ 2.30687, -0.97295, -0.33392],
        [-0.30872,  2.32779, -1.01908],
        [-0.17761, -0.55891,  1.73651],
    ]),
}


def wb_gains_for_ct(ct):
    """Interpolate calibrated WB gains for a given color temperature."""
    cts = sorted(WB_GAIN_TABLE.keys())
    if ct <= cts[0]:
        return WB_GAIN_TABLE[cts[0]]
    if ct >= cts[-1]:
        return WB_GAIN_TABLE[cts[-1]]
    for i in range(len(cts) - 1):
        if cts[i] <= ct <= cts[i + 1]:
            t = (ct - cts[i]) / (cts[i + 1] - cts[i])
            g0 = WB_GAIN_TABLE[cts[i]]
            g1 = WB_GAIN_TABLE[cts[i + 1]]
            return {
                'R': (1 - t) * g0['R'] + t * g1['R'],
                'G': 1.0,
                'B': (1 - t) * g0['B'] + t * g1['B'],
            }
    return WB_GAIN_TABLE[cts[-1]]


def unpack_raw12(data, width, height, stride):
    """Unpack MIPI CSI-2 packed RAW12 to 12-bit uint16 array."""
    raw = np.zeros((height, width), dtype=np.uint16)
    for y in range(height):
        line = data[y * stride : y * stride + (width // 2) * 3]
        line = line.reshape(-1, 3)
        b0 = line[:, 0].astype(np.uint16)
        b1 = line[:, 1].astype(np.uint16)
        b2 = line[:, 2].astype(np.uint16)
        raw[y, 0::2] = (b0 << 4) | (b2 & 0x0F)
        raw[y, 1::2] = (b1 << 4) | ((b2 >> 4) & 0x0F)
    return raw


def subtract_black_level(raw, black_level=BLACK_LEVEL):
    """Subtract black level and normalize to [0, 1] float."""
    img = raw.astype(np.float32) - black_level
    img = np.clip(img, 0, USABLE_RANGE)
    img /= USABLE_RANGE
    return img


def apply_white_balance(img, bayer, gains):
    """Apply per-channel white balance gains on the Bayer mosaic (BEFORE demosaic)."""
    # Map Bayer pattern to channel positions.
    pat = {
        'RGGB': ('R', 'G', 'G', 'B'),
        'BGGR': ('B', 'G', 'G', 'R'),
        'GRBG': ('G', 'R', 'B', 'G'),
        'GBRG': ('G', 'B', 'R', 'G'),
    }[bayer]
    # (0,0), (0,1), (1,0), (1,1)
    positions = {
        pat[0]: (slice(0, None, 2), slice(0, None, 2)),  # even row, even col
        pat[3]: (slice(1, None, 2), slice(1, None, 2)),  # odd row, odd col
    }
    # Green pixels at two positions.
    g_pos1 = (slice(0, None, 2), slice(1, None, 2))  # even row, odd col
    g_pos2 = (slice(1, None, 2), slice(0, None, 2))  # odd row, even col

    img[positions[pat[0]]] *= gains.get(pat[0], 1.0)
    img[positions[pat[3]]] *= gains.get(pat[3], 1.0)
    img[g_pos1] *= gains['G']
    img[g_pos2] *= gains['G']
    np.clip(img, 0, 1, out=img)
    return img


def gray_world_awb(img, bayer):
    """Compute gray-world AWB gains from the Bayer image.
    Uses very low threshold to avoid excluding dim R/B pixels."""
    pat = {
        'RGGB': ('R', 'G', 'G', 'B'),
        'BGGR': ('B', 'G', 'G', 'R'),
        'GRBG': ('G', 'R', 'B', 'G'),
        'GBRG': ('G', 'B', 'R', 'G'),
    }[bayer]

    # Extract each channel.
    ch00 = img[0::2, 0::2]  # pat[0]
    ch01 = img[0::2, 1::2]  # pat[1]
    ch10 = img[1::2, 0::2]  # pat[2]
    ch11 = img[1::2, 1::2]  # pat[3]

    # Only exclude truly black (noise floor) and saturated pixels.
    # R and B channels are much dimmer than G before WB, so a high
    # lo_thresh incorrectly filters them out and biases gains low.
    hi_thresh = 0.95
    lo_thresh = 0.005
    channels = {}
    g_channels = []
    for ch, name in [(ch00, pat[0]), (ch01, pat[1]), (ch10, pat[2]), (ch11, pat[3])]:
        mask = (ch > lo_thresh) & (ch < hi_thresh)
        if mask.sum() < 100:
            mask = np.ones_like(ch, dtype=bool)
        avg = ch[mask].mean()
        print(f"  {name}: mean={avg:.4f} ({mask.sum()} pixels)")
        if name == 'G':
            g_channels.append(avg)
        else:
            channels[name] = avg

    g_avg = np.mean(g_channels) if g_channels else 1.0
    gains = {
        'R': g_avg / channels['R'] if channels.get('R', 0) > 0 else 2.0,
        'G': 1.0,
        'B': g_avg / channels['B'] if channels.get('B', 0) > 0 else 2.0,
    }
    # Clamp to plausible IMX477 range.
    gains['R'] = float(np.clip(gains['R'], 1.0, 5.0))
    gains['B'] = float(np.clip(gains['B'], 1.0, 5.0))
    print(f"AWB gains: R={gains['R']:.2f} G=1.00 B={gains['B']:.2f}")
    return gains


def white_patch_awb(img, bayer, percentile=95):
    """White-patch AWB: use bright (but unsaturated) pixels as white reference.
    Better than gray-world for scenes with white walls/surfaces."""
    pat = {
        'RGGB': ('R', 'G', 'G', 'B'),
        'BGGR': ('B', 'G', 'G', 'R'),
        'GRBG': ('G', 'R', 'B', 'G'),
        'GBRG': ('G', 'B', 'R', 'G'),
    }[bayer]

    ch00 = img[0::2, 0::2]
    ch01 = img[0::2, 1::2]
    ch10 = img[1::2, 0::2]
    ch11 = img[1::2, 1::2]

    channels = {}
    g_vals = []
    for ch, name in [(ch00, pat[0]), (ch01, pat[1]), (ch10, pat[2]), (ch11, pat[3])]:
        # Use the top-percentile value as the "white" reference for this channel.
        val = np.percentile(ch[ch < 0.98], percentile)
        print(f"  {name}: p{percentile}={val:.4f}")
        if name == 'G':
            g_vals.append(val)
        else:
            channels[name] = val

    g_ref = np.mean(g_vals)
    gains = {
        'R': g_ref / channels['R'] if channels['R'] > 0 else 2.0,
        'G': 1.0,
        'B': g_ref / channels['B'] if channels['B'] > 0 else 2.0,
    }
    gains['R'] = float(np.clip(gains['R'], 1.0, 5.0))
    gains['B'] = float(np.clip(gains['B'], 1.0, 5.0))
    print(f"White-patch AWB (p{percentile}): R={gains['R']:.2f} G=1.00 B={gains['B']:.2f}")
    return gains


def demosaic(img, bayer):
    """Demosaic using OpenCV (preferred) or bilinear fallback."""
    try:
        import cv2
        # OpenCV expects uint16 input for best quality.
        img16 = (img * 65535).astype(np.uint16)
        code = {
            'RGGB': cv2.COLOR_BayerRG2BGR,
            'BGGR': cv2.COLOR_BayerBG2BGR,
            'GRBG': cv2.COLOR_BayerGR2BGR,
            'GBRG': cv2.COLOR_BayerGB2BGR,
        }[bayer]
        bgr16 = cv2.demosaicing(img16, code)
        # Convert to float RGB (OpenCV returns BGR).
        rgb = bgr16[:, :, ::-1].astype(np.float32) / 65535.0
        return rgb
    except ImportError:
        return _bilinear_demosaic(img, bayer)


def _bilinear_demosaic(img, bayer):
    """Simple bilinear demosaic fallback."""
    h, w = img.shape
    rgb = np.zeros((h, w, 3), dtype=np.float32)
    # Just assign channels directly (simple nearest-neighbor for now).
    pat = {
        'RGGB': (0, 1, 1, 2),
        'BGGR': (2, 1, 1, 0),
        'GRBG': (1, 0, 2, 1),
        'GBRG': (1, 2, 0, 1),
    }[bayer]
    from scipy.ndimage import uniform_filter
    for ch_idx in range(3):
        plane = np.zeros_like(img)
        mask = np.zeros_like(img)
        for dy in range(2):
            for dx in range(2):
                if pat[dy * 2 + dx] == ch_idx:
                    plane[dy::2, dx::2] = img[dy::2, dx::2]
                    mask[dy::2, dx::2] = 1
        # Interpolate missing pixels.
        plane_sum = uniform_filter(plane, size=3, mode='nearest')
        mask_sum = uniform_filter(mask, size=3, mode='nearest')
        mask_sum = np.maximum(mask_sum, 1e-10)
        rgb[:, :, ch_idx] = np.where(mask > 0, plane, plane_sum / mask_sum)
    return rgb


def apply_ccm(rgb, ccm):
    """Apply 3x3 color correction matrix."""
    h, w, _ = rgb.shape
    flat = rgb.reshape(-1, 3)
    corrected = flat @ ccm.T
    corrected = np.clip(corrected, 0, 1)
    return corrected.reshape(h, w, 3)


def srgb_gamma(img):
    """Apply sRGB gamma curve."""
    out = np.where(
        img <= 0.0031308,
        12.92 * img,
        1.055 * np.power(np.clip(img, 0.0031308, 1.0), 1.0 / 2.4) - 0.055
    )
    return np.clip(out, 0, 1)


def pick_ccm(ct):
    """Pick the nearest CCM from the table, or interpolate between two."""
    cts = sorted(CCM_TABLE.keys())
    if ct <= cts[0]:
        return CCM_TABLE[cts[0]]
    if ct >= cts[-1]:
        return CCM_TABLE[cts[-1]]
    for i in range(len(cts) - 1):
        if cts[i] <= ct <= cts[i + 1]:
            t = (ct - cts[i]) / (cts[i + 1] - cts[i])
            return (1 - t) * CCM_TABLE[cts[i]] + t * CCM_TABLE[cts[i + 1]]
    return CCM_TABLE[cts[-1]]


def main():
    parser = argparse.ArgumentParser(description='Decode IMX477 RAW12 frame')
    parser.add_argument('rawfile', help='Binary RAW12 file (e.g., FRAME.RAW)')
    parser.add_argument('-o', '--output', default=None)
    parser.add_argument('--width', type=int, default=2028)
    parser.add_argument('--height', type=int, default=1520)
    parser.add_argument('--stride', type=int, default=3056)
    parser.add_argument('--bayer', choices=['RGGB', 'BGGR', 'GRBG', 'GBRG'],
                        default=DEFAULT_BAYER)
    parser.add_argument('--no-ccm', action='store_true',
                        help='Skip color correction matrix')
    parser.add_argument('--ct', type=int, default=2848,
                        help='Color temperature for WB+CCM (default: 2848K indoor)')
    parser.add_argument('--wb', choices=['auto', 'gray-world', 'daylight', 'tungsten', 'none'],
                        default='auto',
                        help='WB mode (auto=calibrated from --ct)')
    parser.add_argument('--wb-gains', type=str, default=None,
                        help='Manual WB gains as R,B (e.g., "2.13,2.47")')
    parser.add_argument('--gray', action='store_true',
                        help='Output grayscale only')
    parser.add_argument('--gamma', type=float, default=None,
                        help='Override gamma (default: sRGB curve)')
    args = parser.parse_args()

    if args.output is None:
        from datetime import datetime
        stamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        images_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  '..', 'images')
        os.makedirs(images_dir, exist_ok=True)
        args.output = os.path.join(images_dir, f'capture_{stamp}.png')

    # 1. Read raw data.
    with open(args.rawfile, 'rb') as f:
        data = np.frombuffer(f.read(), dtype=np.uint8)
    expected = args.stride * args.height
    print(f"Read {len(data)} bytes (expected {expected})")
    if len(data) < expected:
        data = np.pad(data, (0, expected - len(data)))

    # 2. Unpack RAW12.
    print("Unpacking RAW12...")
    raw = unpack_raw12(data, args.width, args.height, args.stride)

    # 3. Black level subtraction + normalize.
    img = subtract_black_level(raw)
    print(f"After black level sub: min={img.min():.3f} max={img.max():.3f}")

    if args.gray:
        # Simple grayscale output.
        if args.gamma is not None:
            gray = np.power(img, 1.0 / args.gamma)
        else:
            gray = srgb_gamma(img)
        out = (gray * 255).astype(np.uint8)
        try:
            import cv2
            cv2.imwrite(args.output, out)
        except ImportError:
            from PIL import Image
            Image.fromarray(out, 'L').save(args.output)
        print(f"Saved grayscale: {args.output}")
        return

    # 4. White balance (BEFORE demosaic).
    if args.wb_gains:
        r_g, b_g = [float(x) for x in args.wb_gains.split(',')]
        gains = {'R': r_g, 'G': 1.0, 'B': b_g}
        print(f"WB: manual gains R={r_g:.2f} B={b_g:.2f}")
    elif args.wb == 'auto':
        # Use calibrated sensor gains from libcamera ct_curve for --ct.
        gains = wb_gains_for_ct(args.ct)
        print(f"WB: calibrated for {args.ct}K -> R={gains['R']:.3f} B={gains['B']:.3f}")
    elif args.wb == 'gray-world':
        gains = gray_world_awb(img, args.bayer)
    elif args.wb == 'daylight':
        gains = wb_gains_for_ct(5579)
        print(f"WB: daylight (5579K) R={gains['R']:.2f} B={gains['B']:.2f}")
    elif args.wb == 'tungsten':
        gains = wb_gains_for_ct(2848)
        print(f"WB: tungsten (2848K) R={gains['R']:.2f} B={gains['B']:.2f}")
    else:
        gains = {'R': 1.0, 'G': 1.0, 'B': 1.0}
        print("WB: none")
    img = apply_white_balance(img, args.bayer, gains)

    # 5. Demosaic.
    print(f"Demosaicing ({args.bayer})...")
    rgb = demosaic(img, args.bayer)

    # 6. Color correction matrix.
    if not args.no_ccm:
        ccm = pick_ccm(args.ct)
        print(f"Applying CCM (ct={args.ct}K)")
        rgb = apply_ccm(rgb, ccm)

    # 7. Gamma correction.
    if args.gamma is not None:
        rgb = np.power(np.clip(rgb, 0, 1), 1.0 / args.gamma)
    else:
        rgb = srgb_gamma(rgb)

    # 8. Rotate 180° (sensor is mounted upside down).
    rgb = rgb[::-1, ::-1, :]

    # 9. Save as 8-bit PNG.
    out = (rgb * 255).astype(np.uint8)
    try:
        import cv2
        # Convert RGB to BGR for OpenCV.
        cv2.imwrite(args.output, out[:, :, ::-1])
    except ImportError:
        from PIL import Image
        Image.fromarray(out, 'RGB').save(args.output)
    print(f"Saved: {args.output}")


if __name__ == '__main__':
    main()
