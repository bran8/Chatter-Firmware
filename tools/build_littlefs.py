#!/usr/bin/env python3
"""Build a LittleFS image for the Chatter (headless-wifi build) from ./data.

Why this exists instead of `mklittlefs`:
  The on-device LittleFS library (Arduino `LittleFS_esp32`, lfs v2.4) reads
  *disk version 2.0* and is configured with LFS_NAME_MAX = 64. The bundled
  Arduino `mklittlefs` (4.1.0 / littlefs v2.11.1) writes disk version 2.1 and
  name_max = 255, so its images fail to mount on-device (LFS_ERR_INVAL) and the
  firmware then silently formats a blank FS. This script pins both values to
  match the runtime, producing an image the device can actually mount.

Geometry matches the No-OTA partition (spiffs @ 0x211000, size 0x1EF000) and
esp_littlefs.c config: block 4096, page/read/prog 128, cache 512, lookahead 128.

Usage:
    python tools/build_littlefs.py                 # data/ -> littlefs.bin
    python tools/build_littlefs.py data littlefs.bin

Then flash (No-OTA scheme selected when building the firmware):
    esptool --chip esp32 --baud 921600 write_flash -z 0x211000 littlefs.bin
"""
import os
import sys
from littlefs import LittleFS

# Partition geometry (No-OTA: spiffs size 0x1EF000, ESP32 erase block 4096).
BLOCK_SIZE = 4096
IMAGE_SIZE = 0x1EF000
BLOCK_COUNT = IMAGE_SIZE // BLOCK_SIZE  # 495

# Must match LittleFS_esp32/src/esp_littlefs.c on the device.
READ_SIZE = 128
PROG_SIZE = 128
CACHE_SIZE = 512
LOOKAHEAD_SIZE = 128
NAME_MAX = 64               # CONFIG_LITTLEFS_OBJ_NAME_LEN
DISK_VERSION = 0x00020000   # lfs v2.4 reads disk version 2.0 only

# Files the device never uses and that waste space / confuse listings.
SKIP_NAMES = {".DS_Store", "Thumbs.db"}


def add_dir(fs: LittleFS, host_dir: str, fs_dir: str) -> None:
    for name in sorted(os.listdir(host_dir)):
        if name in SKIP_NAMES:
            continue
        host_path = os.path.join(host_dir, name)
        fs_path = (fs_dir.rstrip("/") + "/" + name) if fs_dir != "/" else "/" + name
        if os.path.isdir(host_path):
            fs.mkdir(fs_path)
            add_dir(fs, host_path, fs_path)
        else:
            with open(host_path, "rb") as src:
                data = src.read()
            with fs.open(fs_path, "wb") as dst:
                dst.write(data)
            print(f"  + {fs_path} ({len(data)} bytes)")


def main() -> int:
    data_dir = sys.argv[1] if len(sys.argv) > 1 else "data"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "littlefs.bin"

    if not os.path.isdir(data_dir):
        print(f"error: data dir '{data_dir}' not found", file=sys.stderr)
        return 1

    fs = LittleFS(
        block_size=BLOCK_SIZE,
        block_count=BLOCK_COUNT,
        read_size=READ_SIZE,
        prog_size=PROG_SIZE,
        cache_size=CACHE_SIZE,
        lookahead_size=LOOKAHEAD_SIZE,
        name_max=NAME_MAX,
        disk_version=DISK_VERSION,
        mount=True,
    )

    print(f"Packing '{data_dir}' -> '{out_path}' "
          f"(disk v2.0, name_max={NAME_MAX}, {BLOCK_COUNT} blocks x {BLOCK_SIZE})")
    add_dir(fs, data_dir, "/")

    with open(out_path, "wb") as f:
        f.write(fs.context.buffer)

    used = BLOCK_SIZE * fs.fs_stat().block_count if hasattr(fs, "fs_stat") else None
    print(f"Wrote {os.path.getsize(out_path)} bytes to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
