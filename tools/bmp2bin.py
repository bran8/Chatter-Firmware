"""
bmp2bin.py -- encode a BMP (or any image Pillow can open) into the Chatter
avatar .bin format: LVGL LV_IMG_CF_INDEXED_8BIT. Reverse of pp.py (bin2bmp).

Output layout matches what src/Elements/Avatar.cpp loads via lv_img_set_src:
  - 4-byte little-endian header (cf=10, width, height packed per lv_img_header_t)
  - 256-entry palette, 4 bytes each, byte order B, G, R, A
  - w*h index bytes, one per pixel, row-major

Note: the firmware's indexed format stores one alpha byte per *palette entry*,
not per pixel, so transparency is quantized to one alpha level per color
during palette-building -- same limitation the original assets have.
"""

import os
import sys
import struct
from PIL import Image

LV_IMG_CF_INDEXED_8BIT = 10
MAX_DIM = 0x7FF  # 11-bit header field


def build_header(w, h, cf=LV_IMG_CF_INDEXED_8BIT):
    if w > MAX_DIM or h > MAX_DIM:
        raise ValueError(f"image too large for the 11-bit LVGL header: {w}x{h} (max {MAX_DIM}x{MAX_DIM})")
    value = (cf & 0x1F) | ((w & 0x7FF) << 10) | ((h & 0x7FF) << 21)
    return struct.pack("<I", value)


def image_to_bin(img):
    img = img.convert("RGBA")
    w, h = img.size

    alpha = img.split()[3]
    rgb = img.convert("RGB")
    quantized = rgb.quantize(colors=256, method=Image.MEDIANCUT)

    palette_rgb = quantized.getpalette()[:256 * 3]
    while len(palette_rgb) < 256 * 3:
        palette_rgb.append(0)

    indices = list(quantized.getdata())
    alpha_data = list(alpha.getdata())

    # Average the source alpha per quantized color, since several source
    # pixels can collapse onto the same palette index.
    alpha_sum = [0] * 256
    alpha_count = [0] * 256
    for idx, a in zip(indices, alpha_data):
        alpha_sum[idx] += a
        alpha_count[idx] += 1

    palette = bytearray()
    for i in range(256):
        r = palette_rgb[i * 3]
        g = palette_rgb[i * 3 + 1]
        b = palette_rgb[i * 3 + 2]
        a = (alpha_sum[i] // alpha_count[i]) if alpha_count[i] else 255
        palette += bytes((b, g, r, a))

    pixel_data = bytes(indices)
    header = build_header(w, h)
    return header + bytes(palette) + pixel_data


def convert_one(input_path, output_path=None):
    img = Image.open(input_path)
    data = image_to_bin(img)
    if output_path is None:
        output_path = os.path.splitext(input_path)[0] + ".bin"
    with open(output_path, "wb") as f:
        f.write(data)
    print(f"{input_path} -> {output_path} ({img.width}x{img.height})")


def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python bmp2bin.py input.bmp [output.bin]")
        print("  python bmp2bin.py --dir <folder>     (convert every .bmp file in folder)")
        sys.exit(1)

    if sys.argv[1] == "--dir":
        folder = sys.argv[2]
        for name in sorted(os.listdir(folder)):
            if name.lower().endswith(".bmp"):
                convert_one(os.path.join(folder, name))
        return

    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    convert_one(input_path, output_path)


if __name__ == "__main__":
    main()
