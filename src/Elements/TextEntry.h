#ifndef CHATTER_FIRMWARE_TEXTENTRY_H
#define CHATTER_FIRMWARE_TEXTENTRY_H

#include <Arduino.h>
#include <lvgl.h>
#include "../Interface/LVObject.h"
#include <string>
#include <vector>
#include <Input/InputListener.h>
#include <Loop/LoopListener.h>

#define EV_ENTRY_DONE ((lv_event_code_t) (_LV_EVENT_LAST + 1))
#define EV_ENTRY_CANCEL ((lv_event_code_t) (_LV_EVENT_LAST + 2))
#define EV_ENTRY_LR ((lv_event_code_t) (_LV_EVENT_LAST + 3))

class TextEntry : public LVObject, private InputListener, public LoopListener {
public:
	TextEntry(lv_obj_t* parent, const std::string& text = "", uint32_t maxLength = -1);
	virtual ~TextEntry();

	void setTextColor(lv_color_t color);
	void setPlaceholder(const std::string& text);
	void setText(const std::string& text);
	std::string getText() const;
	void showCaps(bool show);

    void keyPress(uint8_t i);
    void clear();

    void setCannedMessage(uint8_t button, const std::string& text);
    void clearCannedMessage(uint8_t button);
    void loadCannedMessages();   // populate canned slots from the persistent CannedService

    void start();
	void stop();
	void focus();
	void defocus();
	bool isActive() const;

	void loop(uint micros) override;

private:
	void buttonPressed(uint i) override;
	void buttonReleased(uint i) override;
	void buttonHeld(uint i) override;
	void buttonHeldRepeat(uint i, uint repeatCount) override;

	void backspace();

	static const char* characters[];
	static char* charMap;
    static const std::map<uint8_t, uint8_t> keyMap;

    std::map<uint8_t, std::string> cannedMessages;

    lv_obj_t* entry;
	lv_obj_t* capsText;
	bool active = false;

	lv_group_t* activeGroup = nullptr;
	lv_group_t* inputGroup;

	int8_t currentKey = -1; // currently active key
	uint8_t index = 0; // character under the key
	uint32_t keyTime = 0; // when the key was last pressed

	bool btnRHeld = false;

	uint32_t maxLength; // user-requested max length (the textarea limit is relaxed in T9 mode)

	// Input modes cycled by short-pressing BTN_R: T9 prediction, then the three
	// multi-tap caps levels, then numeric. Replaces the old CapsMode.
	enum InputMode {
		T9, MULTI_LOWER, MULTI_SINGLE, MULTI_UPPER, MULTI_NUMERIC, COUNT
	} inputMode = T9;
	void setInputMode(InputMode mode);

	// Last-used input mode persists to SPIFFS so the user's preferred entry
	// mode (T9 or a multi-tap caps level) is restored for every message.
	static InputMode loadLastInputMode();
	static void saveInputMode(InputMode mode);

	// Quick T9<->aa toggle: the first BTN_R press from T9 jumps to "aa" (for a
	// quick one-off word edit) and the next press returns straight to T9. This
	// detour fires only ONCE per editing session; afterwards BTN_R cycles the
	// deliberate caps modes T9 -> Aa -> AA -> 12 -> T9 (skipping "aa", which is
	// still reached implicitly when "Aa" auto-lowercases after its first letter).
	// Reset in start() so each new edit gets the detour once.
	bool quickReturnUsed = false;

	// ── T9 predictive state ────────────────────────────────────────────────
	std::string confirmedText;             // committed text (no in-progress word)
	std::string t9Digits;                  // digits of the in-progress word
	std::vector<std::string> t9Candidates; // merged custom + dictionary candidates
	int t9MatchIndex = 0;                  // selected candidate

	bool t9PunctActive = false;            // BTN_1 punctuation multi-tap in progress
	uint8_t t9PunctIndex = 0;

	void t9ButtonPressed(uint i);
	void t9AppendDigit(char digit);
	void t9Backspace();
	void t9CommitWord(bool appendSpace);
	void t9CyclePunct();
	void finishPunct();
	void updateTextarea();
	std::string currentT9Word() const;     // selected candidate, auto-capitalised
	bool sentenceStart() const;            // true when the next letter starts a sentence
	void learnLastWord(const std::string& text); // persist the last multi-tap word to CustomDict
	std::vector<std::string> buildCandidates(const std::string& digits) const;
};

#endif //CHATTER_FIRMWARE_TEXTENTRY_H
