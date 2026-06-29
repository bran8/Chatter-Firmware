"""
bin2bmp.py -- decode Chatter avatar .bin files (LVGL LV_IMG_CF_INDEXED_8BIT) to BMP.

File layout (confirmed against src/Elements/Avatar.cpp and the LVGL lv_img_decoder
source the firmware is built against):
  - 4-byte little-endian header (lv_img_header_t bitfield):
        bits 0-4:   cf            (10 = LV_IMG_CF_INDEXED_8BIT)
        bits 5-7:   always_zero
        bits 8-9:   reserved
        bits 10-20: width  (11 bits)
        bits 21-31: height (11 bits)
  - 256-entry palette, 4 bytes each, byte order B, G, R, A
  - w*h index bytes, one per pixel, row-major, each indexing the palette above
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


def save_as_bmp(img, output_path):
    # BMP has no usable alpha channel in Pillow; blend transparency over white
    # so partially/fully transparent avatar corners still look sane.
    bg = Image.new("RGB", img.size, (255, 255, 255))
    bg.paste(img, mask=img.split()[3])
    bg.save(output_path, format="BMP")


def convert_one(input_path, output_path=None):
    img = bin_to_image(input_path)
    if output_path is None:
        output_path = os.path.splitext(input_path)[0] + ".bmp"
    save_as_bmp(img, output_path)
    print(f"{input_path} -> {output_path} ({img.width}x{img.height})")


def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python pp.py input.bin [output.bmp]")
        print("  python pp.py --dir <folder>     (convert every .bin file in folder)")
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
