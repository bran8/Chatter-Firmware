"""
bin2png.py -- decode Chatter avatar .bin files (LVGL LV_IMG_CF_INDEXED_8BIT) to PNG.

Same source format as bin2bmp.py (see that file for the full byte layout), but
emits a true-color PNG with the alpha channel preserved. The avatars are
circular with transparent corners, so PNG (unlike the flattened 24-bit BMP) keeps
those corners transparent -- they composite correctly over the web page's dark
background instead of showing as white squares.
"""

import os
import sys
import struct
from PIL import Image

LV_IMG_CF_INDEXED_8BIT = 10
PALETTE_BYTES = 256 * 4


def parse_header(data):
    value = struct.unpack("<I", data[:4])[0]
    cf = value & 0x1F
    w = (value >> 10) & 0x7FF
    h = (value >> 21) & 0x7FF
    return cf, w, h


def bin_to_image(path):
    with open(path, "rb") as f:
        data = f.read()

    cf, w, h = parse_header(data)
    if cf != LV_IMG_CF_INDEXED_8BIT:
        raise ValueError(f"{path}: unsupported color format {cf} (expected {LV_IMG_CF_INDEXED_8BIT}=INDEXED_8BIT)")

    palette_raw = data[4:4 + PALETTE_BYTES]
    pixel_data = data[4 + PALETTE_BYTES:4 + PALETTE_BYTES + w * h]
    if len(pixel_data) < w * h:
        raise ValueError(f"{path}: file too short for a {w}x{h} indexed image")

    img = Image.new("RGBA", (w, h))
    pixels = []
    for idx in pixel_data:
        b, g, r, a = palette_raw[idx * 4:idx * 4 + 4]
        pixels.append((r, g, b, a))
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
        print("  python bin2png.py input.bin [output.png]")
        print("  python bin2png.py --dir <folder>     (convert every .bin file in folder)")
        sys.exit(1)

    if sys.argv[1] == "--dir":
        folder = sys.argv[2]
        for name in sorted(os.listdir(folder)):
            if name.lower().endswith(".bin"):
                convert_one(os.path.join(folder, name))
        return

    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    convert_one(input_path, output_path)


if __name__ == "__main__":
    main()
