# Plan: MainMenu Wrap-around & Settings Retry Time Update

## Implementation Status
- **COMPLETED**: Throttled LoRa Packet Debugging, [>> Broadcast All] feature.
- **IN PROGRESS**: MainMenu cursor wrap-around, Settings menu retry time options.

## What Was Implemented

### 1. [>> Broadcast All]
- Implemented in `src/UI/InboxScreen.cpp`.
- Broadcasts messages to all connected nodes.

### 2. Throttled LoRa Packet Debugging
- **File**: `src/Services/MessageService.cpp`
- Added non-blocking debug logging in `loop()` every 5 seconds.
- Logs packet type (`TEXT`, `PIC`, `ACK`, `OTHER`), sender UID, and content info.

### 3. Cleanup
- Deleted temporary `PacketMonitorScreen` files.

## Current Implementation Tasks (In Progress)

### 1. MainMenu Cursor Wrap-around
- **Goal**: Safe menu looping in `src/Screens/MainMenu.cpp`.
- **Changes**:
  - Update `selectNext()` and `selectPrev()` to wrap `selected` index around `ItemCount`.
  - Repurpose/remove `arrowHideAnim` logic at boundaries so arrows do not disappear (top/bottom are connected).
  - Manage `lv_obj_scroll_to` during wrap jumps using `LV_ANIM_OFF` to prevent jarring full-list scroll animations.

### 2. Settings Menu Retry Time
- **Goal**: Modify retry time capability in the Settings menu.
- **New Options**: 15 seconds, 1 minute, 2 minutes, 5 minutes.
- **Files**: Target Settings menu implementation files.

## Next Steps
1. Finalize boundary checks and LVGL animation handling in `MainMenu`.
2. Implement and test retry time cycling in Settings menu.
3. Validate UI stability after menu wrapping.

## Key Decisions
- Wrap-around uses index modulo logic for continuity.
- Wrap jumps disable LVGL scroll animations to prevent visual glitches.
- Debug interval fixed at 5 seconds to minimize radio interference.

## Relevant Files
- `src/Screens/MainMenu.cpp`: Menu looping logic.
- `src/Screens/MainMenu.h`: Menu class definition.
- `src/Services/MessageService.cpp`: Debug logging.
- `src/UI/InboxScreen.cpp`: Broadcast All feature.
- `src/Screens/BroadcastScreen.cpp`: Broadcast logic reference.
