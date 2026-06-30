# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

**Chatter 2.0 Green** is a custom firmware fork for the Chatter device — a peer-to-peer encrypted wireless communicator built on the ESP32 microcontroller with LoRa radio. The codebase is written in C++/Arduino and uses LVGL (Light and Versatile Graphics Library) for the UI.

### Key Features

- **Broadcast messaging** – send one message to all friends
- **T9 predictive text** – faster text entry using digit sequences
- **Retry-until-ACK protocol** – reliable message delivery with acknowledgments
- **Incoming message notifications** – continuous sound until acknowledged
- **Canned responses** – long-press to select pre-written replies
- **Games** – embedded space invaders, snake, and other games
- **Wrap-around menus** – improved menu navigation

---

## Hardware & Platform

- **Microcontroller**: ESP32-WROOM-32 (dual-core, 240 MHz, 520 KB SRAM)
- **Radio**: LLCC68 LoRa transceiver (low-bandwidth, encrypted peer-to-peer)
- **Display**: Color LCD via SPI
- **Input**: 10-key numeric keypad + control buttons (directional, backspace, shift)
- **Storage**: Flash + SPIFFS (for assets and user data)
- **Audio**: Piezoelectric buzzer via PWM tone generation

---

## Build & Deployment

### Using Arduino IDE 2.3.4+ (Windows)

1. **Install CircuitMess board support**:
   
   - Preferences → Add Board Manager URL:
     
     ```
     https://raw.githubusercontent.com/CircuitMess/Arduino-Packages/master/package_circuitmess.com_esp32_index.json
     ```
   - Tools → Board → Boards Manager → search "circuitmess" → install ESP32 Boards v1.8.3

2. **Build and upload**:
   
   - Open `Chatter-Firmware.ino`
   - Tools → Board → **Chatter 2.0 by CircuitMess**
   - Tools → Port → select USB COM port (typically COM4)
   - Click Verify (checkmark) to compile — takes ~2 minutes
   - Click Upload (arrow) to flash to device
   - Note: There's a known conflict warning about `Sprite::pushImage` — does not prevent upload

3. **Select partition scheme**: Tools → Partition Scheme → **"No OTA"** (LittleFS lives at
   `0x211000` in this scheme; the default scheme puts it at `0x291000` and the image below
   would not be mounted). Flash the firmware with this selected.

4. **Upload LittleFS assets** (Arduino 2.X):

   - **Do NOT use Arduino's `mklittlefs`** — it writes disk version 2.1 / name_max 255, which
     the on-device `LittleFS_esp32` library (lfs v2.4, disk 2.0, name_max 64) cannot mount.
     The firmware then formats a blank FS each boot (broken images, profile resets).
   - Build the image with the bundled script (pins disk v2.0 + name_max 64):
     `python tools/build_littlefs.py data littlefs.bin` (`pip install littlefs-python` once)
   - Upload with esptool: `esptool --chip esp32 --baud 921600 write_flash -z 0x211000 littlefs.bin`
   - See `SETUP.md` for the full partition table and rationale.

### Using CMake (Windows/Linux/macOS)

```bash
# Configure
mkdir cmake && cd cmake
cmake ..

# Build (compiles with arduino-cli)
cmake --build . --target CMBuild

# Upload to device (requires PORT set in CMakeLists.txt)
cmake --build . --target CMUpload
```

---

## Source Tree Structure

```
src/
├── Chatter-Firmware.ino          # Entry point; initializes LVGL, services, and loop
├── Storage/                       # Repo<T> pattern for persistent data
│   ├── Repo.h/.cpp              # Generic storage layer; maps to SPIFFS files
│   ├── Storage.h/.cpp           # Entity definitions (Friend, Message, Convo)
│   └── [other entity files]
├── Services/                      # Non-blocking background tasks
│   ├── LoRaService.h/.cpp       # Low-level radio SPI + encryption
│   ├── MessageService.h/.cpp    # App-level send/receive, ACK, broadcast
│   ├── BuzzerService.h/.cpp     # Non-blocking tone scheduler
│   ├── ProfileService.h/.cpp    # User profile (nickname, avatar)
│   └── [other services]
├── Screens/                       # Main UI controller screens
│   ├── MainMenu.h/.cpp          # Root menu dispatcher
│   ├── ConvoView.h/.cpp         # Conversation thread display
│   └── [other screens]
├── Interface/                     # LVGL wrapper and screen/modal base classes
│   ├── LVScreen.h/.cpp          # Base class for screens
│   ├── LVModal.h/.cpp           # Modal dialog base
│   └── Pics.h/.cpp              # Image asset loader
├── Elements/                      # UI components
│   ├── TextEntry.h/.cpp         # Multi-tap & T9 text input handler
│   ├── ConvoBox.h/.cpp          # Message bubble renderer
│   ├── Avatar.h/.cpp            # User avatar display
│   └── [other elements]
├── Model/                         # Entity definitions (Friend, Message, etc.)
├── Modals/                        # Dialog components
├── Games/                         # Game engine & games (Snake, Space, Invaders)
│   └── GameEngine/              # ECS-style game loop
├── t9_dict.*                      # Compiled T9 dictionary (binary search)
├── t9_dict.bin                    # Binary T9 data (163 KB)
├── ChatterTheme.h/.cpp           # LVGL theme colors & styles
├── InputChatter.h/.cpp           # Keypad ISR & debounce
├── InputLVGL.h/.cpp              # LVGL input driver adapter
└── FSLVGL.h/.cpp                 # File system integration with LVGL
```

---

## Key Architectural Concepts

### 1. Event-Driven Loop System

- **Entry point**: `Chatter-Firmware.ino` calls `loop()` repeatedly
- **Registration**: Components inherit from `LoopListener` and register with `LoopManager`
- **Periodic ticks**: Each tick fires `loop(uint micros)` on all listeners
- **Button events**: Low-level ISRs + debouncing fire `ButtonEvent` to active input listeners
- **Non-blocking**: Never call `delay()`; use elapsed time checks with `millis()`

### 2. Storage Repo Pattern

Located in `src/Storage/Repo.h/.cpp`:

- **Generic**: `Repo<T>` where `T` is an Entity (Friend, Message, Convo)
- **Persistent**: Entities serialize to individual SPIFFS files (e.g., `/repo/messages/<uid>`)
- **Caching**: Open files are cached in `std::unordered_map` to avoid FS lookups
- **File limit**: Too many open files will overflow the FD table — close when done
- **No bulk delete**: Only per-record deletion; use soft deletes (`deleted_at` flag) where needed

### 3. Service Layer

Services handle background tasks and don't block the main loop:

- **LoRaService**: Physical packet SPI, encryption, link-level ACKs
- **MessageService**: App-level broadcast, retry-until-ACK, message sequencing
- **BuzzerService**: Schedules tones without blocking; checked every loop tick
- **ProfileService**: In-memory user profile; syncs to NVS on change
- **SleepService**: Power management; wakes on button/radio events

### 4. UI Hierarchy

- **Screens** (e.g., `MainMenu`, `ConvoView`): Full-screen views; register with loop manager
- **Modals** (e.g., emoji picker, settings): Overlay on top of screen
- **Elements** (e.g., TextEntry, Avatar): Reusable components; emit custom LVGL events
- **LVGL**: All rendering; color/font/style definitions in `ChatterTheme.cpp`

### 5. Text Input

- **Multi-tap** (src/Elements/TextEntry.cpp): Numeric keys cycle through letters (2=abc2, 3=def3, etc.)
- **T9 lookup**: Uses binary search on `t9_dict` array to find word candidates
- **Timeout**: 1-second key inactivity advances to next character
- **Cursor visual**: Custom rendering for text preview and candidate list

---

## Memory & Hardware Constraints

### RAM (Limited)

- ESP32 has only 520 KB SRAM; dynamically allocated structures (std::vector, std::string) can fragment
- **Repo caching**: Keep open file handles minimal; close when done
- **Asset loading**: LVGL image descriptors point to SPIFFS directly; avoid loading entire images into RAM

### SPIFFS (Flat filesystem)

- No nested directories; file search is O(N) with total file count
- Avoid large single files; prefer segmented messages
- Use `Repo` caching layer to minimize FS lookups

### LoRa (Extreme bandwidth constraint)

- **Link speed**: ~50 bits/sec (very slow)
- **Message segmentation**: Large messages split into packets
- **Retry overhead**: Retry-until-ACK loops can monopolize the channel
- **Channel sharing**: Multiple devices compete for airtime; implement backoff

### CPU (Cooperative multitasking)

- No preemption; long-running code blocks all other tasks
- Avoid nested loops or recursive operations
- Use loop tick callbacks for periodic work (e.g., sensor reading, battery check)

---

## Development Guidelines

### Code Organization

- **Headers (.h)**: Minimal includes; use forward declarations where possible
- **Implementation (.cpp)**: Include only what's needed; avoid bloat
- **Namespacing**: Use `src::` prefix for internal modules where appropriate
- **Naming**: CamelCase for classes/types, snake_case for variables/functions

### Non-Blocking Patterns

```cpp
// BAD: Blocks entire loop
void processMessage() {
    delay(100);  // Never!
}

// GOOD: Check elapsed time
uint32_t lastCheck = 0;
void loop(uint micros) {
    if (millis() - lastCheck >= 100) {
        lastCheck = millis();
        processMessage();
    }
}
```

### Storage Access

```cpp
// Add/update entity
Friend f;
f.uid = some_uid;
f.profile.nickname = "Alice";
Storage.Friends.add(f);

// Retrieve entity
Friend f = Storage.Friends.get(some_uid);

// Iterate all
for (UID_t uid : Storage.Friends.all()) {
    Friend f = Storage.Friends.get(uid);
}

// Delete (removes from repo)
Storage.Friends.remove(some_uid);
```

### Custom LVGL Events

```cpp
// Emit event from element
lv_event_send(obj, EV_ENTRY_DONE, nullptr);

// Listen in screen
void eventCb(lv_event_t* e) {
    if (e->code == EV_ENTRY_DONE) { /*...*/ }
}
lv_obj_add_event_cb(element, eventCb, EV_ENTRY_DONE, nullptr);
```

---

## Key Files & Modules

- **src/Storage/Repo.h** – Generic storage abstraction (read for understanding data persistence)
- **src/Services/LoRaService.h** – Radio protocol (edit carefully; affects all wireless comm)
- **src/Services/MessageService.h** – Message application logic (edit for broadcast, ACK, retry behavior)
- **src/Elements/TextEntry.cpp** – Multi-tap & T9 input (high complexity; test thoroughly)
- **src/Games/GameEngine/** – ECS-style game loop (modular; safe to add new games)
- **docs/architecture_and_specifics.md** – In-depth architecture notes; read before major refactors

---

## Common Tasks

### Adding a new screen

1. Inherit from `LVScreen` in `src/Interface/LVScreen.h`
2. Implement `void createUI()` (called on screen creation)
3. Implement `void loop(uint micros)` for updates
4. Register with `LoopManager` in constructor
5. Add to `MainMenu` dispatcher

### Adding a new service

1. Inherit from `LoopListener`
2. Implement `void loop(uint micros)` for periodic work
3. Initialize in `Chatter-Firmware.ino`
4. Call service methods from screens/elements

### Modifying T9 dictionary

1. Edit **tools/8293.txt** to add/remove words
2. Run script to regenerate **src/t9_dict.cpp** and **src/t9_dict.bin**
3. Recompile firmware

### Testing changes locally

- Use Arduino IDE's built-in Serial Monitor to print debug output
- Set `Serial.begin(115200)` in setup; use `Serial.println()` for logging
- Enable `#define DEBUG` in CMakeLists.txt for debug builds

---

## Troubleshooting

### Build fails with "cannot find -larduino"

- Check CircuitMess board package is installed (v1.8.3+)
- Verify CMakeLists.txt PORT and DEVICE settings

### LittleFS upload fails

- Ensure `mklittlefs` and `esptool` are in PATH
- Check LittleFS partition size in Arduino boards.txt (key: `<device>.menu.PartitionScheme.min_spiffs.upload.maximum_size`)
- Verify flash address (0x211000 for Chatter 2.0 with min_spiffs partition)

### Device crashes / reboots unexpectedly

- Check for stack overflow (too much local variable allocation)
- Check for heap fragmentation (use `ESP.getFreeHeap()` to debug)
- Check for file descriptor exhaustion (Repo caching issue)
- Review recent LoRa message volume (channel saturation)

### T9 lookup returns no results

- Check t9_dict.cpp is properly compiled (not excluded from build)
- Verify word exists in source wordlist (case-sensitive)
- Binary search is strict; typos will not match

---

## References

- [LVGL 8 Documentation](https://docs.lvgl.io/8.3/)
- [Arduino ESP32 Docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [CircuitMess Chatter Library](https://github.com/CircuitMess/Chatter-Library)
- [LoRa Radio Documentation](https://lora-alliance.org/)
- Local docs: `docs/architecture_and_specifics.md`, `docs/WiFi.md`, `docs/lora_settings_guide.md`
