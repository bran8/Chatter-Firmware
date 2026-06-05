# Plan: Add "[>> Broadcast All]" menu option to InboxScreen & Debugging LoRa Packets

## Implementation Status
**COMPLETED - Throttled LoRa Packet Debugging**

### What Was Implemented

### 1. Throttled Serial Debugging in `MessageService`
- **File**: `src/Services/MessageService.cpp`
- **Action**: Added debug logging in `loop()` function
- **Details**:
  - Debug prints once every 5 seconds (non-blocking, throttled)
  - Logs packet type, sender UID, and basic content info:
    - `TEXT` packets: prints message length
    - `PIC` packets: prints picture index
    - `ACK` packets: prints message UID
    - Other/unknown types: printed as "OTHER"
  - Minimal overhead: only 1 check + printf every 5 seconds, no string building for every packet
  - Non-blocking: uses `millis()` for timing, no delays

### 2. `PacketMonitorScreen` (DELETED - Not Needed)
- Files created temporarily but removed since the simpler serial logging approach was preferred
- Would have been a full UI screen in the Games menu to view raw packet data

## Original Goals (Updated)

### 1. [>> Broadcast All] Feature
- **Status**: Not implemented in current change
- **Notes**: User requested this later, but implementation focused on Packet Monitor debugging first

### 2. Debugging LoRa Packets
- **Status**: IMPLEMENTED
- **Approach**: Throttled serial logging for non-intrusive debugging
- **Location**: `MessageService::loop()` function
- **Benefits**:
  - No impact on normal operation when not checking serial monitor
  - Debug interval is long (5s) so radio packet processing isn't affected
  - Simple printf to Serial, minimal overhead
  - Can be extended to show more packet details as needed

## Implementation Steps (Original Plan - Updated)

### 1. Update `MessageService`
- **COMPLETED**: Added throttled debug logging in `MessageService::loop`
  - Uses `millis()` for non-blocking timing
  - Only prints debug info once every 5 seconds
  - Logs packet type, sender, and basic content info

### 2. Packet Monitor Screen (DELETED)
- Not implemented - simpler serial logging approach chosen

### 3. Games Screen Entry (NOT APPLIED)
- The Packet Monitor entry was not added to GamesScreen since the direct serial logging approach was preferred

## Notes
- The debug logging is designed to be "set it and forget it" - it will print debug info every 5 seconds if the serial monitor is open
- When serial monitor is closed, the overhead is minimal (just one `millis()` call every 5 seconds)
- The 5-second interval was chosen to be non-intrusive while still providing periodic debugging capability
- To change the debug interval, modify `DEBUG_INTERVAL_MS` constant in `MessageService::loop()`
