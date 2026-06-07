# Plan: MainMenu Wrap-around & Settings Retry Time Update

T9 Predictive Text — Design Implementation Brief
1. Dictionary Generation (Python tooling)
Create tools/gen_t9_dict.py that reads tools/t9_wordlist.txt (one word per line, frequency-ordered best-first), converts each word to its T9 digit sequence using the classic phone mapping (ABC→2, DEF→3, GHI→4, JKL→5, MNO→6, PQRS→7, TUV→8, WXYZ→9), and stores each entry as a (uint64_t digits, uint16_t word_offset) pair. After processing all words, sort the entry array by digits ascending, then by original word-list position (i.e. frequency rank) as a tiebreaker so that within any collision group the best word comes first. Write a flat binary file src/t9_dict.bin with a 4-byte header (uint16_t entry_count, uint16_t string_table_size), followed by the sorted entry array, followed by a null-terminated string table of all words concatenated. Also emit src/t9_dict.h declaring extern const uint8_t t9DictData[] and the two header constants. The script should finish by running a self-verification pass — look up digit sequences for a handful of known words (hello→43556, good→4663, the→843) and assert the correct word is the first result, failing loudly if not.
2. Firmware Lookup Module
Create src/Elements/T9Dict.h and src/Elements/T9Dict.cpp as a self-contained lookup layer that the rest of the firmware calls and that knows nothing about LVGL or input handling. On first use, T9Dict::init() reads the embedded t9DictData[] array, parses the header, and stores a pointer to the entry array and string table — no heap copies, just pointer arithmetic into the flash-resident array. Provide two public methods: const char* T9Dict::bestMatch(uint64_t digits) which binary-searches the entry array for the first entry with that digit key and returns the corresponding string, or nullptr if none; and int T9Dict::matchCount(uint64_t digits) which counts contiguous entries sharing that key. A third method const char* T9Dict::matchAt(uint64_t digits, int index) walks index steps forward from the lower-bound result, enabling prediction cycling without any heap allocation. All three methods are O(log n + k) at worst and realistically sub-microsecond on ESP32.
3. TextEntry Integration
In TextEntry.h, add three private members: bool t9Mode = false, std::string t9Digits (accumulates the raw digit string for the current word, e.g. "43556"), and int t9MatchIndex = 0 (which prediction within the collision group is currently shown). In TextEntry.cpp, modify buttonHeld() on BTN_R to toggle t9Mode, clear t9Digits, reset t9MatchIndex, and swap the caps indicator label text between the existing caps strings and a fixed "T9" string. In keyPress(), add an early branch: if t9Mode is true, map the button through keyMap to get the digit character ('2'–'9'), append it to t9Digits, convert t9Digits to a uint64_t key, call T9Dict::bestMatch(), and if a result exists, delete the characters for the previous in-progress word from the textarea (i.e. delete t9Digits.size() - 1 characters) and insert the new best match — all with LV_ANIM_OFF. Skip the currentKey/index/keyTime/loop() machinery entirely in this branch. In buttonPressed(), when t9Mode is active, intercept BTN_UP and BTN_DOWN to increment/decrement t9MatchIndex (clamped to matchCount - 1), recompute the uint64_t key from t9Digits, call matchAt(), and replace the in-progress word in the textarea the same way. Handle BTN_L in T9 mode by popping the last character from t9Digits and re-running the lookup, or clearing the in-progress word if t9Digits becomes empty. In buttonReleased() on BTN_R (short press, i.e. !btnRHeld), if t9Mode is active, commit the current word by appending a space to the textarea, clearing t9Digits, and resetting t9MatchIndex to zero rather than cycling caps.
4. Wiring, Caps, and Canned Messages
Include t9_dict.h in TextEntry.cpp and call T9Dict::init() once — either lazily on first t9Mode activation or at construction time. Guard all cannedMessages long-hold paths in buttonHeld() with if(!t9Mode) so that digit keys don't accidentally fire canned inserts mid-word. Similarly, wrap the setCapsMode() cycling logic in buttonReleased() with if(!t9Mode) so BTN_R short-press commits in T9 context rather than changing case. For capitalization in T9 mode, after committing a word check whether the textarea is empty or the last non-space character is a sentence-ending punctuation mark ('.', '!', '?'), and if so capitalize the first letter of the inserted word using the existing toUpperCase() helper before inserting it into the textarea. When stop() is called, do not reset t9Mode — persist the mode preference across sessions so the user doesn't have to re-enable it each time they open a text entry. The loop() timer callback should check if(t9Mode) return at the top to prevent the 1-second idle commit from firing during T9 word composition.

## Implementation Status
- **COMPLETED**: , [>> Broadcast All] feature.
- **IN PROGRESS**: Settings menu retry time options.

## What Was Implemented


### 2. Throttled LoRa Packet Debugging
- **File**: `src/Services/MessageService.cpp`
- Added non-blocking debug logging in `loop()` every 5 seconds.
- Logs packet type (`TEXT`, `PIC`, `ACK`, `OTHER`), sender UID, and content info.

### 3. Cleanup
- Deleted temporary `PacketMonitorScreen` files.

## Current Implementation Tasks (In Progress)


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
