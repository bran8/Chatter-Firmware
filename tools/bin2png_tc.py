"""
bin2png_tc.py -- decode Chatter Pics .bin files (LVGL LV_IMG_CF_TRUE_COLOR, RGB565) to PNG.

Header: 4-byte little-endian uint32
  bits [4:0]   = color format (4 = LV_IMG_CF_TRUE_COLOR)
  bits [20:10] = width in pixels
  bits [31:21] = height in pixels
Pixel data: width*height 16-bit LE values, RGB565 packed.

Usage:
  python tools/bin2png_tc.py data/Pics/0.bin [output.png]
  python tools/bin2png_tc.py --dir data/Pics
"""

import os
import sys
import struct
from PIL import Image

LV_IMG_CF_TRUE_COLOR = 4


def parse_header(data):
    value = struct.unpack("<I", data[:4])[0]
    cf = value & 0x1F
    w = (value >> 10) & 0x7FF
    h = (value >> 21) & 0x7FF
    return cf, w, h


def rgb565_to_rgb888(pixel):
    r = ((pixel >> 11) & 0x1F) << 3
    g = ((pixel >> 5) & 0x3F) << 2
    b = (pixel & 0x1F) << 3
    # fill low bits
    r |= r >> 5
    g |= g >> 6
    b |= b >> 5
    return r, g, b


def bin_to_image(path):
    with open(path, "rb") as f:
        data = f.read()

    cf, w, h = parse_header(data)
    if cf != LV_IMG_CF_TRUE_COLOR:
        raise ValueError(f"{path}: unsupported color format {cf} (expected {LV_IMG_CF_TRUE_COLOR}=TRUE_COLOR)")

    pixel_data = data[4:4 + w * h * 2]
    if len(pixel_data) < w * h * 2:
        raise ValueError(f"{path}: file too short for {w}x{h} RGB565 image")

    pixels = []
    for i in range(0, len(pixel_data), 2):
        pixel = struct.unpack_from("<H", pixel_data, i)[0]
        pixels.append(rgb565_to_rgb888(pixel))

    img = Image.new("RGB", (w, h))
    img.putdata(pixels)
    return img


def convert_one(input_path, output_path=None):
    img = bin_to_image(input_path)
    if output_path is None:
        output_path = os.path.splitext(input_path)[0] + ".png"
    img.save(output_path, format="PNG")
    print(f"{input_path} -> {output_path} ({img.width}x{img.height})")


def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python tools/bin2png_tc.py input.bin [output.png]")
        print("  python tools/bin2png_tc.py --dir <folder>")
        sys.exit(1)

    if sys.argv[1] == "--dir":
        folder = sys.argv[2]
        for name in sorted(os.listdir(folder)):
            if name.lower().endswith(".bin"):
                convert_one(os.path.join(folder, name))
        return

    convert_one(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)


if __name__ == "__main__":
    main()
