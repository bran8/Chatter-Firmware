from PIL import Image

def rgb565_to_png(binary_filepath, width, height, output_filepath):
    with open(binary_filepath, 'rb') as f:
        binary_data = f.read()

    # Create image from raw buffer. 
    # Use "BGR;16" or "RGB;16" depending on the endianness of your binary data
    img = Image.frombuffer("RGB", (width, height), binary_data, "raw", "BGR;16", 0, 1)
    
    img.save(output_filepath, "PNG")

# Example usage:
# rgb565_to_png('image.bin', 320, 240, 'output.png')



def manual_rgb565_to_png(binary_filepath, width, height, output_filepath):
    with open(binary_filepath, 'rb') as f:
        binary_data = f.read()

    img = Image.new('RGB', (width, height))
    index = 0

    for y in range(height):
        for x in range(width):
            # Read 2 bytes (16 bits) per pixel
            low = binary_data[index]
            high = binary_data[index + 1]
            pixel = (high << 8) | low
            
            # Extract R, G, B
            r = (pixel >> 11) & 0x1F
            g = (pixel >> 5) & 0x3F
            b = pixel & 0x1F
            
            # Scale to 8-bit color (0-255)
            r8 = (r * 527 + 23) >> 6
            g8 = (g * 259 + 33) >> 6
            b8 = (b * 527 + 23) >> 6
            
            img.putpixel((x, y), (r8, g8, b8))
            index += 2

    img.save(output_filepath, "PNG")

import struct
from PIL import Image

def convert_lvgl_rgb565(file_path, output_path, width=34, height=41, swap_bytes=True):
    with open(file_path, "rb") as f:
        # Skip the 4-byte LVGL header (cf, reserved, w, h bits)
        header = f.read(4) 
        raw_data = f.read()

    # Create a new RGB image
    img = Image.new("RGB", (width, height))
    pixels = img.load()

    idx = 0
    for y in range(height):
        for x in range(width):
            if idx >= len(raw_data):
                break
                
            # Read 2 bytes for RGB565
            b1 = raw_data[idx]
            b2 = raw_data[idx+1]
            
            # If colors look "faint" or wrong, toggle the byte order
            pixel_value = (b2 << 8) | b1 if swap_bytes else (b1 << 8) | b2
            
            # Extract channels
            r = (pixel_value >> 11) & 0x1F
            g = (pixel_value >> 5) & 0x3F
            b = pixel_value & 0x1F

            # Scale to 8-bit color depth (0-255)
            r8 = (r * 527 + 23) >> 6
            g8 = (g * 259 + 33) >> 6
            b8 = (b * 527 + 23) >> 6

            pixels[x, y] = (r8, g8, b8)
            idx += 2

    img.save(output_path, "PNG")
    print(f"Saved successfully as {output_path}")

# Try changing swap_bytes to False if colors are still distorted


import struct
from PIL import Image

def fix_arduino_lvgl_image(file_path, output_path, width=34, height=41):
    with open(file_path, "rb") as f:
        # Step 1: Strip out the 4-byte LVGL container header
        header = f.read(4) 
        raw_bytes = f.read()

    # Step 2: Ensure we pair the lingering byte arrays properly
    img = Image.new("RGB", (width, height))
    pixels = img.load()

    byte_idx = 0
    for y in range(height):
        for x in range(width):
            if byte_idx + 1 >= len(raw_bytes):
                break
                
            b1 = raw_bytes[byte_idx]
            b2 = raw_bytes[byte_idx+1]
            
            # BIG-ENDIAN SWAP: Forces correct Arduino screen rendering
            # Instead of (b2 << 8) | b1, we merge them in reverse order
            pixel = (b1 << 8) | b2
            
            # Step 3: Unpack the true RGB565 bit sequences
            r = (pixel >> 11) & 0x1F
            g = (pixel >> 5) & 0x3F
            b = pixel & 0x1F

            # Step 4: Scale 5/6-bit depths cleanly up to 8-bit true color
            r8 = int((r * 255) / 31)
            g8 = int((g * 255) / 63)
            b8 = int((b * 255) / 31)

            pixels[x, y] = (r8, g8, b8)
            byte_idx += 2

    img.save(output_path, "PNG")
    print(f"Successfully exported to {output_path}")

# Run the Big-Endian fix on the true resolution profile
fix_arduino_lvgl_image("1.bin", "fixed_arduino_image.png", width=34, height=41)
