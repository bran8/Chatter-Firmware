# Chatter 2.0 Green

This fork adds my custom tweaks for Chatter 2.0.  Agentic Engineering assisted, peer review welcome.

**Key improvements:**

- **Broadcast to all friends** – send one message that goes to all your friends
- *T9* - Basic version of T9 word entry using unrefined frequency wordlist 
- **Retry until Ack** – all sends are retried until acknowledged by receiver (Go to Settings to clear the sending queue to clear the airwaves)
- **Incoming Annoyance** – incoming messages are important, play sound continuiously until silenced
- **Canned responses** – Long Press to type a pre-canned message (Go to Settings to customize)

Quality of Life

- Wrap around menus (sacrificing some animation)
- Keep draft message

Detailed Setup that worked for me: 

I deploy firmware from a Windows 11 PC running Arduino IDE version 2.3.4   (https://www.arduino.cc/en/software/)  

On first use, I opened Preferences and added this URL to the Boards Manager:  

https://raw.githubusercontent.com/CircuitMess/Arduino-Packages/master/package_circuitmess.com_esp32_index.json

Click Tools -> Board -> Boards Manager...    (or CTRL+Shift+B)

Search for "circuitmess"

Install 1.8.3 for ESP32 Boards by CircuitMess

Then on the Select Board pull-down selector, you can find the "Boards" called: **Chatter 2.0 by CircuitMess**

On my system, it connected to COM4 Serial Port (USB)

Click File->Open-> **Chatter-Firmware.ino**

Click the "Checkmark" to verify.  It will "Compiling sketch..." for a few minutes.

I get a conflict error with overlapping candidates; void Sprite::pushImage and void lgfx::v1::LGFXBase::pushImage.  It does not stop me from Uploading the sketch to the device successfully.

If you need to factory restore the device, login to https://code.circuitmess.com/ and click on Restore Firmware in the top-right corner. 

## Serial Monitor

Set Baud to 115200

# Chatter Firmware

> The repository for the core firmware that comes preloaded on every Chatter.

Put together and code two encrypted wireless communicators.

# Compiling

The firmware is based on the [Chatter Library](https://github.com/CircuitMess/Chatter-Library).

The library is included with other device libraries in the CircuitMess ESP32 Arduino platform.

More info and installation instructions
on [CircuitMess/Arduino-Packages](https://github.com/CircuitMess/Arduino-Packages).

## Using Arduino IDE

### Installing the package

Follow the package installation
instructions [here](https://github.com/CircuitMess/Arduino-Packages?tab=readme-ov-file#installation).

### Building

Open Chatter-Firmware.ino using Arduino IDE, set the board to Chatter, and compile.

## Using CMake

To compile and upload, you need to have [CMake](https://cmake.org/)
and [arduino-cli](https://github.com/arduino/arduino-cli) installed. You also need to have both of
them registered in the PATH.

In the CMakeLists.txt file, change the port to your desired COM port (default is /dev/ttyUSB0):

```
set(PORT /dev/ttyUSB0)
```

Then in the root directory of the repository type:

```
mkdir cmake
cd cmake
cmake ..
cmake --build . --target CMBuild
```

This will compile the binaries and place the .bin and .elf files in the build/ directory located in
the root of the repository.

To compile the binary and upload it according to the port set in CMakeLists.txt, run

```
cmake --build . --target CMBuild
```

in the cmake directory.

# Uploading SPIFFS

The ESP32 contains a Serial Peripheral Interface Flash File System (SPIFFS). SPIFFS is a
lightweight filesystem created for microcontrollers with a flash chip.

Here are stored UI and audio assets used in the firmware.

## Using the Arduino ESP32 filesystem uploader plugin (only for Arduino 1.X)

Install the [plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin) following the instructions
from
the [README.md](https://github.com/me-no-dev/arduino-esp32fs-plugin?tab=readme-ov-file#installation)

Then from the opened sketch select Tools > ESP32 Sketch Data Upload menu item. This should start
uploading the files into ESP32 flash filesystem.

## Using the mkspiffs utility

When building with CMake or Arduino 2.X, you will need to build and upload the SPIFFS image
separately.

First, download the latest [mkspiffs](https://github.com/igrr/mkspiffs) utility for your OS with
the "-arduino-esp32" suffix. (For
example, [mkspiffs-0.2.3-arduino-esp32-win32.zip](https://github.com/igrr/mkspiffs/releases/download/0.2.3/mkspiffs-0.2.3-arduino-esp32-win32.zip)
for Windows).

Then create the binary SPIFFS image using the command in the root of the project:

```
mkspiffs -c data -s 0x1EF000 -b 4096 -p 256 spiffs.bin
```

The block size (-b) and page size (-p) parameters should stay as-is.

The size parameter (-s) can be determined from the board-specific SPIFFS partition size, which
can be found in the
platform [boards.txt](https://github.com/CircuitMess/Arduino-ESP32/blob/master/boards.txt) under
`<device>.menu.PartitionScheme.min_spiffs.upload.maximum_size`

For uploading the image, you will need to download [esptool](https://github.com/espressif/esptool).

Then, flash the compiled image to the board.

The SPIFFS partition address will be defined alongside the SPIFFS partitions size, under
`<device>.menu.PartitionScheme.min_spiffs.upload.spiffs_start`.
This parameter is used in the
following esptool command call (before referencing the built image):

```
esptool --chip esp32 --baud 921600  --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x211000 spiffs.bin
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
