# Chatter 2.0 Green (Headless WiFi)

This fork adds my custom tweaks for Chatter 2.0.  Agentic Engineering assisted, peer review welcome.

**Key improvements:**

- **Connect to Chatter over WiFi** - use your phone

- **Broadcast to all friends** – send one message that goes to all your friends

- **Retry until Ack** – all sends are retried until acknowledged by receiver (Go to Settings to clear the sending queue to clear the airwaves)

- **Incoming Annoyance** – incoming messages are important, play sound continuiously until silenced

Detailed Setup that worked for me: 

I deploy firmware from a Windows 11 PC running Arduino IDE version 2.3.4   (https://www.arduino.cc/en/software/)  

On first use, I opened Preferences and added this URL to the Boards Manager:  

https://raw.githubusercontent.com/CircuitMess/Arduino-Packages/master/package_circuitmess.com_esp32_index.json

Click Tools -> Board -> Boards Manager...    (or CTRL+Shift+B)

Search for "circuitmess"

Install 1.8.3 for ESP32 Boards by CircuitMess

Then on the Select Board pull-down selector, you can find the "Boards" called: **Chatter 2.0 by CircuitMess**

On my system, it connected to COM4 Serial Port (USB)

Click Tools -> Manage Libraries...

Search for "LittleFS_esp32 by lorol"...  Pick version 1.0.6 and install...

Click File->Open-> **Chatter-Firmware.ino**

Click the "Checkmark" to verify.  It will "Compiling sketch..." for a few minutes.

Next, you need to upload the **data** to the flash, you may use the included `littlefs.bin` for convenience or read further below to create your own.

Download the esptool https://github.com/espressif/esptool/releases 

```
esptool --chip esp32 --baud 921600  --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x211000 littlefs.bin
```

If you need to factory restore the device, login to https://code.circuitmess.com/ and click on Restore Firmware in the top-right corner. 

## Serial Monitor

Set Baud to 115200

# Uploading the LittleFS image

The ESP32 stores UI and audio assets in a LittleFS partition (this fork migrated off
SPIFFS for better flash wear leveling). The image is built from `data/` and flashed
separately from the firmware.

## Partition scheme (read this first)

The firmware mounts LittleFS at a partition whose address depends on the **Partition
Scheme** selected when you compile, and the image **must be flashed to that same address**:

| Scheme (Tools > Partition Scheme) | LittleFS offset | LittleFS size |
| --------------------------------- | --------------- | ------------- |
| **No OTA** (use this build)       | **0x211000**    | **0x1EF000**  |
| Default (chatter2 default)        | 0x291000        | 0x16F000      |

Select **Tools > Partition Scheme > "No OTA"** before flashing the firmware so the
running app mounts LittleFS at `0x211000`, matching the flash address below. If the
firmware and the image disagree on the address, the mount fails and the firmware
formats a blank filesystem on every boot (broken images, profile resets each boot).

## Building the LittleFS image (use build_littlefs.py, NOT mklittlefs)

> **Do not use the Arduino-bundled `mklittlefs`.** Version 4.1.0 (littlefs v2.11.1)
> writes an image with **on-disk version 2.1** and **name_max 255**. The on-device
> library (`LittleFS_esp32`, lfs v2.4) only reads **disk version 2.0** and is built
> with **name_max 64**, so mklittlefs images fail to mount (`LFS_ERR_INVAL`) and the
> firmware silently formats a blank FS. The symptoms are missing assets and a profile
> that regenerates on every boot.

Build the image with the bundled script, which pins disk version 2.0 and name_max 64
to match the runtime (one-time setup: `pip install littlefs-python`):

```
python tools/build_littlefs.py data littlefs.bin
```

The geometry (block 4096, size 0x1EF000 = 495 blocks) and the disk version / name_max
are baked into the script to match `LittleFS_esp32/src/esp_littlefs.c` and the No-OTA
partition; see the comments in `tools/build_littlefs.py` if the partition layout or
on-device library version ever changes.

For uploading the image, you will need to download [esptool](https://github.com/espressif/esptool).

Then, flash the compiled image to the board.

The partition address can be found under
`<device>.menu.PartitionScheme.min_spiffs.upload.spiffs_start`.
This parameter is used in the
following esptool command call (before referencing the built image):

```
esptool --chip esp32 --baud 921600  --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x211000 littlefs.bin
```

# Restoring the stock firmware

There are three main ways to restore the stock firmware:

### 1) Restoring using esptool

For uploading the firmware this way, you will need to
download [esptool](https://github.com/espressif/esptool).

Then download the prebuilt binary on
the [releases page](https://github.com/CircuitMess/Chatter-Firmware/releases) of this repository
and flash it manually using esptool:

```shell
esptool write_flash 0x0 Codee-Firmware.bin
```

### 2) Restoring using Arduino's burn bootloader option

This Arduino option is usually reserved for bootloader flashing.

For devices included in the CircuitMess ESP32 Arduino platform this will actually restore the
firmware.

Open this project in Arduino and select your board in the `Tools > Board` dropdown menu.

Then select the appropriate firmware under `Tools > Programmer` and click the `Tools > Burn 
bootloader` option.

### 3) Restoring using CircuitBlocks

[CircuitBlocks](https://code.circuitmess.com/) is our educational block-based coding platform.

You can also restore your firmware here by logging in, clicking the "Restore Firmware" button in the top-right corner,
and following the on-screen instructions.

---

Copyright © 2025 CircuitMess

Licensed under [MIT License](https://opensource.org/licenses/MIT).
