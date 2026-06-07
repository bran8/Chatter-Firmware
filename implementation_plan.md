# T9 Input System Implementation Plan (Version 2.1)

Implement a predictive text input system supporting both T9 Predictive Mode and Multi-tap Mode for the ESP32-based Chatter device.

---

## Pre-Implementation Checklist

> [!WARNING]
> Complete every item in this checklist before writing any firmware code. Each item is a concrete action with a clear done state.

---

### A. Verify LVGL Recolor Support

**Why:** The T9 unconfirmed-word highlight uses LVGL inline recolor syntax (`#808080 word#`). If the flag is off, the raw markup will appear as literal text on screen.

- [ ] Open `lv_conf.h` (or the platform-level LVGL config included by CircuitOS/Chatter BSP).
- [ ] Search for `LV_USE_LABEL_RECOLOR`.
- [ ] If it is `0`, change it to `1` and commit the change before any other work.
- [ ] If the flag does not exist, find the LVGL version in use and confirm the equivalent mechanism.

**Done when:** `LV_USE_LABEL_RECOLOR` is `1` in the active config and the project compiles cleanly.

---

### B. Decide BTN_0 Behaviour in T9 Mode

**Why:** `keyMap` currently maps `BTN_0` → index 9 (`" 0"` — space and zero). In T9 mode, `BTN_RIGHT` is already spacebar/confirm, so `BTN_0`'s role is ambiguous.

- [ ] Choose one of:
  - **Option 1 (Recommended):** In T9 mode, `BTN_0` is ignored (no-op). The existing multi-tap space/zero is only available in multi-tap modes.
  - **Option 2:** In T9 mode, `BTN_0` acts identically to `BTN_RIGHT` (confirm current word + space).
- [ ] Write the chosen behaviour into the `TextEntry.cpp` implementation section below before coding begins.

**Done when:** A single option is selected and recorded in the Proposed Changes section.

---

### C. Decide BTN_1 Word-Commit Order in T9 Mode

**Why:** `BTN_1` cycles punctuation (`.,?!+-:()*1`). If an in-progress T9 word exists when BTN_1 is pressed, the order of operations is undefined in the current plan.

- [ ] Adopt this rule (change only if there is a strong reason):
  > In T9 mode, if `t9Digits` is non-empty when BTN_1 is pressed, call `commitCurrentWord()` first (appends the top prediction without a trailing space), then begin punctuation multi-tap. When punctuation is committed, the result is `<word><punctuation>` with no extra space.
- [ ] Record this rule in the `TextEntry.cpp` implementation section below.

**Done when:** The rule is written into the Proposed Changes section.

---

### D. Fix `gen_t9_dict.py` — Remove the Broken `build_entries()` Call

**Why:** `main()` calls `build_entries(words)` on line 349, but the function is never defined. The script cannot run. This is a blocker; the existing `src/t9_dict.h` was produced by an older version.

- [ ] Open `tools/gen_t9_dict.py`.
- [ ] Delete the call `entries, string_table = build_entries(words)` and the surrounding `serialise`/`emit_header` calls that use the old flat format.
- [ ] The replacement is the BFS Trie serialiser described in the Proposed Changes section — do not write a stub `build_entries()`; remove the call entirely.
- [ ] After the rewrite, confirm the script runs end-to-end: `python tools/gen_t9_dict.py` should produce a new `src/t9_dict.h` with no errors.

**Done when:** `python tools/gen_t9_dict.py` exits with code 0 and prints node count + size.

---

### E. Verify Trie Size Fits in Flash

**Why:** The existing flat dict is 142 KB in DRAM. A BFS Trie over the same vocabulary (8 293 words mapped to digits 2–9) is estimated at 20 000–35 000 nodes × 8 bytes = 160–280 KB. This fits in Flash (4 MB) but must not land in DRAM.

- [ ] After running the updated `gen_t9_dict.py`, check the printed node count and total byte size.
- [ ] Confirm the size is under 512 KB (reasonable Flash limit for this blob given other firmware).
- [ ] If node count exceeds 35 000, reduce the word list (`8293.txt`) or cap word length at 10 characters in the script.

**Done when:** Node count is printed and confirmed under budget.

---

### F. Move Dict Data Out of the Header (Prevent DRAM Duplication)

**Why:** `static const uint8_t t9DictData[]` in a `.h` file creates a separate copy in every `.cpp` that includes it, silently consuming DRAM. ESP32 has ~320 KB of DRAM; the dict alone would consume most of it.

- [ ] In `gen_t9_dict.py`, change `emit_header()` so that `t9DictData` is emitted into a **separate** `src/t9_dict.cpp` file (not inside the header) as a plain `const uint8_t t9DictData[] = { ... };` (no `static`).
- [ ] In `src/t9_dict.h`, replace the array definition with `extern const uint8_t t9DictData[]; extern const uint32_t t9DictSize;`.
- [ ] Update the Proposed Changes section for `gen_t9_dict.py` below to reflect that the tool now emits two output files: `src/t9_dict.h` and `src/t9_dict.cpp`.

**Done when:** `src/t9_dict.h` contains only `extern` declarations; the array body is in `src/t9_dict.cpp`.

---

### G. Resolve Custom Dictionary Architecture — Remove Cross-Layer Dependency

**Why:** The Proposed Changes section currently says "In `T9Dict::getMatches`, check the custom dictionary first." `T9Dict` is in `src/Elements/` and `MessageService` is in `src/Services/`. Elements must not depend on Services; doing so creates an include cycle.

- [ ] Update `T9Dict::getMatches` specification: it queries **only** the static trie. No custom dict logic inside `T9Dict`.
- [ ] Add a `CustomDict` owner to `TextEntry`:
  - `TextEntry` holds `std::unordered_map<std::string, uint32_t> customDict` (loaded from SPIFFS on first `start()`).
  - `TextEntry` exposes `void addWord(const std::string& word)` which increments the map and saves to `/custom_dict.txt`.
- [ ] Update the `MessageService.cpp` Proposed Changes section: after send/receive, call `textEntry.addWord(word)` (or a global `CustomDictService` — see below) rather than writing to `T9Dict`.
- [ ] Decide ownership: if `TextEntry` is the only consumer, it owns the map. If other parts of the system need it, extract a `CustomDictService` singleton in `src/Services/`. Record the decision here before coding.
- [ ] Update the merge logic in `TextEntry::keyPress`: after `T9Dict::getMatches(t9Digits)`, iterate `customDict` for words whose digit encoding matches `t9Digits`, prepend them sorted by frequency, then append trie results.

**Done when:** `T9Dict.h` has no `#include` of anything from `Services/`; custom dict merging happens in `TextEntry.cpp`.

---

### H. Clarify `CapsMode` Removal and `InputMode` Takeover

**Why:** `TextEntry` currently has `enum CapsMode { LOWER, SINGLE, UPPER, COUNT }` with `setCapsMode()`, used in `stop()`, `loop()`, `buttonHeld()`, and `buttonReleased()`. The new `InputMode` replaces this entirely, but the plan does not say so explicitly, risking a partial rewrite that leaves dead code or breaks the caps indicator.

- [ ] In `TextEntry.h`: remove `CapsMode` enum and `capsMode` field entirely.
- [ ] Replace with `InputMode` and a `setInputMode(InputMode)` method.
- [ ] Map display strings: `{ T9→"T9", MULTI_LOWER→"aa", MULTI_SINGLE→"Aa", MULTI_UPPER→"AA", MULTI_NUMERIC→"123" }`.
- [ ] In `stop()`: replace the `if(capsMode == SINGLE)` reset with `if(inputMode == MULTI_SINGLE) setInputMode(MULTI_LOWER)`.
- [ ] In `loop()` (the multi-tap timeout handler): replace `if(capsMode == SINGLE)` with `if(inputMode == MULTI_SINGLE)`.
- [ ] `showCaps()` / `capsText` label stays — it now shows the `InputMode` string instead of the `CapsMode` string.

**Done when:** No reference to `CapsMode`, `capsMode`, or `setCapsMode` remains in the codebase.

---

### I. Specify Custom Dictionary File Format

**Why:** The plan references `/custom_dict.txt` on SPIFFS but never defines what the file contains. Without a format spec, the load/save code cannot be written consistently.

- [ ] Adopt this format (one entry per line, tab-separated):
  ```
  hello\t7
  world\t3
  hey\t12
  ```
  Fields: `<word>\t<count>\n`. Words are lowercase, alphabetic only. Lines with any other format are skipped on load.
- [ ] Record this format in the `MessageService.cpp` Proposed Changes section below.

**Done when:** The format is written into the Proposed Changes section.

---

### J. Establish Implementation Order

**Why:** The three features are independent. Implementing them in order of size/risk prevents a large broken branch.

- [ ] **Phase 1:** `BatteryElement.cpp` — replace image with voltage label. Single file, zero dependencies, compile-testable immediately.
- [ ] **Phase 2:** `BuzzerService` + `SettingsScreen` keypad sound switch — validates the SPIFFS settings read/write pattern used later by the custom dict.
- [ ] **Phase 3:** T9 system — `gen_t9_dict.py` rewrite → `t9_dict.h/cpp` → `T9Dict.h/cpp` → `TextEntry` integration → `MessageService` custom dict.

Each phase ends with a clean compile and a commit before the next phase begins.

---

## Implementation Status (2026-06-06)

All code has been written. Firmware could **not** be compiled locally (`arduino-cli`
is not installed on this machine — build/upload is the user's manual step), so the
C++ was verified by inspection against the actual LVGL v8 / library headers. The
Python generator was run and fully verified.

### Resolutions to the checklist above

| Item | Resolution |
|------|-----------|
| A — LVGL recolor flag | **N/A.** Project is LVGL **v8**, where label recolour is built into `LV_USE_LABEL` (no separate `LV_USE_LABEL_RECOLOR`). `lv_label_set_recolor()` exists and is enabled on the textarea's label. |
| B — BTN_0 in T9 | **Decided:** BTN_0 confirms the current word and appends a space (consistent with 0 = space in multi-tap). |
| C — BTN_1 commit order | **Implemented:** in T9, BTN_1 commits the in-progress word (no trailing space) first, then runs punctuation multi-tap on `.,?!+-:()*1`. |
| D — broken `build_entries()` | **Fixed.** Script rewritten; `python tools/gen_t9_dict.py` runs clean (exit 0). |
| E — trie size | **Verified:** 20 458 nodes, **159.8 KB** in Flash. Within budget (< 64 K node uint16 limit, < 512 KB). |
| F — Flash vs DRAM | **Done.** Generator now emits `src/t9_dict.cpp` (array body, lands in `.rodata`) and `src/t9_dict.h` (only `extern` decls + `#define`s). |
| G — custom dict layering | **Revised & improved.** The dict is NOT owned by the transient `TextEntry` (messages arrive when no TextEntry exists). It is a standalone global `CustomDict` (`src/Services/CustomDictService.*`). Flow: `MessageService --learnText--> CustomDict <--getMatches-- TextEntry`. No Elements↔Services cycle; `T9Dict` stays pure. |
| H — CapsMode removal | **Done.** `CapsMode`/`capsMode`/`setCapsMode` fully removed; replaced by `InputMode`/`setInputMode`. Verified no other file references them. |
| I — custom dict format | **Decided:** `word<TAB>count\n` per line, lowercase alphabetic words ≥ 2 letters. |
| J — order | Followed: Battery → Keypad sound → T9. |

### Files written

* `tools/gen_t9_dict.py` — rewritten as a frequency **character**-trie generator (run + verified).
* `src/t9_dict.h` / `src/t9_dict.cpp` — generated (extern decl + Flash blob).
* `src/Elements/T9Dict.h` / `.cpp` — pure trie lookup (predictive prefix matching).
* `src/Services/CustomDictService.h` / `.cpp` — learned-word dictionary (new global `CustomDict`).
* `src/Elements/BatteryElement.h` / `.cpp` — voltage label.
* `src/Services/BuzzerService.h` / `.cpp` — keypad-sound toggle + `emitBeep()`.
* `src/Screens/SettingsScreen.h` / `.cpp` — "Keypad sound" switch.
* `src/Elements/TextEntry.h` / `.cpp` — `InputMode` + T9 predictive entry.
* `src/Services/MessageService.cpp` — `CustomDict.learnText()` on send/receive.
* `Chatter-Firmware.ino` — `CustomDict.begin()` after `Storage.begin()`.

---

## Open Ambiguities — Need Your Decision / On-Device Validation

> [!IMPORTANT]
> These could not be resolved from the code alone. Items 1–3 are the important ones.

1. **No physical Up/Down buttons (hardware fact).** `Pins.hpp` aliases `BTN_UP→BTN_LEFT`
   and `BTN_DOWN→BTN_RIGHT` — they are the *same* buttons. The plan's "BTN_UP/BTN_DOWN
   cycle candidates while BTN_RIGHT = space" is therefore physically impossible.
   **Implemented mapping in T9 mode:** LEFT/RIGHT **cycle prediction candidates**;
   **BTN_0 = confirm word + space**; BTN_L = backspace; BTN_1 = punctuation; BTN_R = mode cycle.
   *Consequence:* in T9 mode LEFT/RIGHT no longer move the text cursor or emit
   `EV_ENTRY_LR`, and mid-word cursor editing is unavailable (T9 always appends at the
   end). **Confirm this control scheme is acceptable**, or tell me a different mapping.

2. **Predictive (prefix) matching vs classic exact-length T9.** `T9Dict::getMatches`
   returns every word whose encoding *starts with* the typed digits (ranked by
   frequency), not only words of the exact typed length. This matches the grey
   "predicted completion" UX in the spec, but means the candidate list can mix word
   lengths (e.g. `4663` → good, gone, home, hood, **and** goodbye…). **Confirm you want
   predictive completion**, or switch to exact-length collision groups.

3. **Grey recolour inside the textarea — needs a visual check on device.** To make
   `lv_textarea_set_text` preserve the `#808080 …#` markup, T9 mode clears the
   textarea's `accepted_chars` and `max_length` (otherwise LVGL re-adds the text
   char-by-char and strips the `#`). Verified against the LVGL v8 source, but please
   confirm on device that the prediction renders **grey**, not as literal `#808080`.

4. **Length limit in T9 is approximate.** With `accepted_chars`/`max_length` relaxed,
   the limit is enforced on `confirmedText` length in `t9AppendDigit()` rather than by
   the widget. Fine for the 60-char message case; revisit if a tight limit matters.

5. **Invalid-key beep follows the keypad-sound setting.** `emitBeep()` is silent when
   keypad sounds are OFF (and requires master `Settings.sound`). If you want the error
   beep to sound even with keypad clicks disabled, say so.

6. **Keypad sound is independent of master Sound.** A keypad click needs BOTH
   `Settings.sound` ON **and** the new keypad toggle ON. Confirm that's the intended
   relationship (vs. keypad toggle overriding master).

7. **Custom dictionary growth is unbounded** and is rewritten to SPIFFS on every
   message containing a new/changed word. Vocabulary is small in practice, but consider
   a max-entry cap / prune policy if storage or write wear becomes a concern.

---

## User Review Required

> [!IMPORTANT]
> The dictionary will use an optimized character-based prefix tree (Trie) serialized in BFS order and stored directly in Flash. Each node will contain a pre-calculated `max_weight` representing the maximum weight of any word in its subtree. This allows prediction lookups to run in $O(L)$ time (where $L$ is the typed word length) without any runtime recursive subtree scanning.

> [!NOTE]
> Keypad sounds setting will be stored in a dedicated text configuration file on SPIFFS (`/keypad_sound.txt`) and added as a switch in the Settings Screen. This ensures settings persist across reboots without modifying external system libraries.

## Proposed Changes

### Dictionary Generation Tool

#### [MODIFY] [gen_t9_dict.py](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/tools/gen_t9_dict.py)
* Remove the broken `build_entries()` call from `main()` along with all flat-format serialisation code. Do not add a stub — delete the call.
* Rewrite the script to build a character prefix tree (Trie) from the word list `8293.txt` where each edge represents a **T9 digit** (2–9), not a character.
* Calculate the `max_weight` for each node (maximum weight of any word in its subtree).
* Serialize the Trie using BFS (Breadth-First Search) so that all child nodes of any parent node are contiguous in the output array.
* Emit **two** output files (see checklist item F):
  * `src/t9_dict.h` — contains only `extern` declarations and `#define` constants. No array body.
  * `src/t9_dict.cpp` — contains the `const uint8_t t9DictData[] = { ... };` array body (no `static`). The linker places this in Flash (`.rodata`).
* Print node count and total byte size on stdout so it can be verified against Flash budget (see checklist item E).
* Binary layout of the array written into `src/t9_dict.cpp`:
  * Header (4 bytes):
    * `uint16_t node_count` (2 bytes)
    * `uint16_t reserved` (2 bytes)
  * Node Array: `node_count` × 8 bytes:
    * `char character` (1 byte) — the T9 digit character ('2'–'9'), or `\0` for the root
    * `uint8_t child_count` (1 byte)
    * `uint16_t weight` (2 bytes, rank of the word ending at this node; 0 if no word ends here)
    * `uint16_t max_weight` (2 bytes, max weight in this subtree)
    * `uint16_t first_child_index` (2 bytes, BFS index of first child; 0 if no children)

---

### Firmware Lookup Module

#### [NEW] [T9Dict.h](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Elements/T9Dict.h)
* Define the packed `TrieNode` structure matching the binary layout exactly:
  ```cpp
  struct __attribute__((packed)) TrieNode {
      char     character;       // T9 digit '2'–'9', or '\0' for root
      uint8_t  child_count;
      uint16_t weight;          // word rank (0 = no word ends here)
      uint16_t max_weight;      // max weight in subtree
      uint16_t first_child_idx; // BFS index of first child
  };
  ```
* Declare:
  * `void T9Dict::init()` — sets internal pointer to `t9DictData + 4` (skipping the 4-byte header) and reads `node_count` from the header.
  * `std::vector<std::pair<std::string, uint16_t>> T9Dict::getMatches(const std::string& digits)` — returns `(word, weight)` pairs for all complete words reachable by the given digit sequence, sorted by `weight` descending. Does **not** access `MessageService` or any custom dictionary (see checklist item G).

#### [NEW] [T9Dict.cpp](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Elements/T9Dict.cpp)
* Implement `getMatches`:
  1. Walk the trie from the root one digit at a time. At each node, find the child whose `character` matches the next digit. If no child matches, return empty.
  2. At the terminal node (all digits consumed), collect complete words from the subtree using a BFS/DFS up to a practical limit (e.g. 16 candidates).
  3. A node is a complete word when `node.weight > 0`; its word string must be reconstructed by recording characters along the path (the trie stores digits on edges, so the lookup must map digit paths back to character strings — **see note below**).
  4. Return candidates sorted by `weight` descending.

> [!NOTE]
> **Word string reconstruction:** The BFS-serialised trie stores T9 digits on edges, not the original characters. To return the actual word string from `getMatches`, either (a) store a word-string offset in each leaf node pointing into a string table appended after the node array, or (b) reconstruct the word from `confirmedText` context in `TextEntry`. **Option (a) is strongly preferred** — add a `uint16_t string_offset` field to `TrieNode` (making nodes 10 bytes), or store the string table separately after the node array and index it by `weight` rank. Decide and record the choice in this section before coding `T9Dict.cpp`.

---

### UI & Input Integration

#### [MODIFY] [TextEntry.h](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Elements/TextEntry.h)
* Remove `enum CapsMode` and `capsMode` field entirely (see checklist item H).
* Declare:
  * `enum InputMode { T9, MULTI_LOWER, MULTI_SINGLE, MULTI_UPPER, MULTI_NUMERIC, COUNT };`
  * `InputMode inputMode` (replaces `capsMode`)
  * `void setInputMode(InputMode mode)` (replaces `setCapsMode()`; updates `capsText` label)
  * `std::string t9Digits` — digit sequence of the current in-progress word
  * `int t9MatchIndex` — index into `t9Candidates` of the currently displayed prediction
  * `std::vector<std::pair<std::string, uint16_t>> t9Candidates` — `(word, weight)` pairs from `T9Dict::getMatches` merged with custom dict matches
  * `std::string confirmedText` — all finalized text, not including the in-progress prediction
  * `std::unordered_map<std::string, uint32_t> customDict` — in-RAM custom word frequencies loaded from SPIFFS (see checklist item G)
  * `void loadCustomDict()` — reads `/custom_dict.txt` from SPIFFS into `customDict`
  * `void saveCustomDict()` — writes `customDict` back to `/custom_dict.txt`
  * `void addCustomWord(const std::string& word)` — increments word in `customDict` and calls `saveCustomDict()`
  * `void updateTextarea()` — redraws textarea: `confirmedText + "#808080 <prediction>#"` while unconfirmed, plain `confirmedText + word` once confirmed
  * `void commitCurrentWord(bool appendSpace)` — appends the current top prediction to `confirmedText` (with trailing space if `appendSpace`), resets `t9Digits`, `t9MatchIndex`, `t9Candidates`

#### [MODIFY] [TextEntry.cpp](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Elements/TextEntry.cpp)
* In constructor: initialize `inputMode` to `T9`; call `T9Dict::init()`; call `loadCustomDict()`.
* In `start()`: do not reset `inputMode` — preserve it across sessions.
* In `stop()`:
  * Replace `if(capsMode == SINGLE) setCapsMode(LOWER)` with `if(inputMode == MULTI_SINGLE) setInputMode(MULTI_LOWER)`.
  * If `t9Digits` is non-empty, call `commitCurrentWord(false)` to preserve the in-progress word.
* In `loop()` (the multi-tap timeout): replace `if(capsMode == SINGLE)` with `if(inputMode == MULTI_SINGLE)`.
* Enable LVGL recoloring on the textarea's internal label widget on construction (requires `LV_USE_LABEL_RECOLOR 1` — see checklist item A).
* `buttonReleased(BTN_R)`:
  * If not held: cycle `inputMode` through `T9 → MULTI_LOWER → MULTI_SINGLE → MULTI_UPPER → MULTI_NUMERIC → T9`.
  * When cycling **away** from `T9`: call `commitCurrentWord(false)`.
  * When cycling **into** `T9`: reset `t9Digits`, `t9MatchIndex`, `t9Candidates`.
* `buttonPressed(BTN_1)` in T9 mode:
  * If `t9Digits` is non-empty: call `commitCurrentWord(false)` first, then begin punctuation multi-tap (see checklist item C).
  * If `t9Digits` is empty: begin punctuation multi-tap immediately using the existing multi-tap timer mechanism with the character set `.,?!+-:()*1`.
* `buttonPressed(BTN_2` to `BTN_9)` in T9 mode:
  * Append the corresponding digit character to `t9Digits`.
  * Merge candidates: call `T9Dict::getMatches(t9Digits)` then prepend matching `customDict` words (words whose `word_to_digits()` equals `t9Digits`, sorted by frequency descending).
  * If merged candidate list is empty: call `Buzz.emitBeep()`, remove the last appended digit from `t9Digits`, restore previous candidates — do not update the textarea.
  * Otherwise: set `t9MatchIndex = 0`, call `updateTextarea()`.
* `buttonPressed(BTN_0)` in T9 mode: **no-op** (see checklist item B).
* `buttonPressed(BTN_UP)` / `buttonPressed(BTN_DOWN)` in T9 mode: cycle `t9MatchIndex` through `t9Candidates` (wrapping), call `updateTextarea()`.
* `buttonPressed(BTN_L)` in T9 mode: if `t9Digits` non-empty, remove last character, rebuild candidates from new `t9Digits`, call `updateTextarea()`. If `t9Digits` is empty, delete last character from `confirmedText` directly.
* `buttonPressed(BTN_RIGHT)` in T9 mode: call `commitCurrentWord(true)` (appends space).
* Auto-capitalization in `updateTextarea()`: if `confirmedText` is empty, or its last non-space character is `.`, `!`, or `?`, capitalize the first letter of the displayed prediction.

---

### Custom Dictionary Integration

#### [MODIFY] [MessageService.cpp](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Services/MessageService.cpp)
* **Note:** The custom dictionary is owned by `TextEntry`, not `MessageService` (see checklist item G). `MessageService` does not hold or save the dict map directly.
* In `sendText()` and `receiveMessage()`, after the message is processed, parse the text into lowercase alphabetic words and call `textEntryRef->addCustomWord(word)` for each word — **or** emit a callback/event that `TextEntry` listens to.
* The `/custom_dict.txt` file format is `word<TAB>count\n` per line (see checklist item I). `MessageService` does not read or write this file; `TextEntry::loadCustomDict()` and `TextEntry::saveCustomDict()` own the file I/O.
* Determine how `MessageService` will reference `TextEntry` (direct pointer set at init, or a listener interface) and record the choice here before coding.

---

### Keypad Sound & Battery Status

#### [MODIFY] [BuzzerService.h](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Services/BuzzerService.h)
* Declare `bool keypadSoundsEnabled`.
* Declare `void emitBeep()`.

#### [MODIFY] [BuzzerService.cpp](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Services/BuzzerService.cpp)
* Load `keypadSoundsEnabled` from `/keypad_sound.txt` on boot.
* Guard the tone playback in `buttonPressed` with `if (keypadSoundsEnabled)`.
* Implement `emitBeep()` to play a low error beep (e.g. 150Hz tone for 100ms) on invalid T9 keystrokes.

#### [MODIFY] [SettingsScreen.h](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Screens/SettingsScreen.h)
* Add `lv_obj_t* keypadSound` and `lv_obj_t* keypadSoundSwitch`.

#### [MODIFY] [SettingsScreen.cpp](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Screens/SettingsScreen.cpp)
* Render the "Keypad sound" ON/OFF switch.
* Load the state from SPIFFS and save it back when the switch is toggled.

#### [MODIFY] [BatteryElement.cpp](file:///c:/Users/subran/Documents/Scripts/Chatter-Firmware/src/Elements/BatteryElement.cpp)
* Remove the `img` object.
* Create a text label `label` and format it with the raw voltage using `Battery.getVoltage()` (e.g., `4.1V`).

## Verification Plan

### Implementation Order (per checklist item J)
1. **Phase 1 — Battery display:** `BatteryElement.cpp` only. Compile and verify.
2. **Phase 2 — Keypad sound:** `BuzzerService` + `SettingsScreen`. Compile and verify.
3. **Phase 3 — T9 system:** dictionary tool → dict files → `T9Dict` → `TextEntry` → `MessageService` integration.

### Automated Tests
* **Step 1:** Regenerate the dictionary (must be done before compiling):
  ```powershell
  python tools/gen_t9_dict.py
  ```
  Expected output includes node count (target: under 35 000) and total size (target: under 512 KB). Verify the script exits with code 0.
* **Step 2:** Verify clean compilation of firmware:
  ```powershell
  arduino-cli compile --fqbn cm:esp32:chatter Chatter-Firmware.ino
  ```

### Manual Verification
* Deploy the compiled firmware to the ESP32 device.
* Verify that:
  1. The battery indicator displays text voltage (e.g., "4.1V") instead of an icon.
  2. The input mode starts in T9 and cycles through T9 -> aa -> Aa -> AA -> 123 on short press of BTN_R.
  3. Predictive text displays unconfirmed words in grey and confirmed words in black.
  4. Pressing Spacebar or Key '1' punctuation confirms the word.
  5. Invalid T9 combinations beep and keep the last valid prediction.
  6. Incoming/outgoing message words get added to the custom dictionary and are prioritized in future T9 inputs.
  7. The Settings menu has a "Keypad sound" ON/OFF switch that operates correctly and persists.
