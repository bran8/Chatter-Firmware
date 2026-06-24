#ifndef CHATTER_FIRMWARE_CANNEDEDITSCREEN_H
#define CHATTER_FIRMWARE_CANNEDEDITSCREEN_H

#include <Arduino.h>
#include <cstddef>
#include "../Interface/LVScreen.h"
#include "../Elements/TextEntry.h"
#include <Input/InputListener.h>

/**
 * Single-slot editor for a canned message. Opens a full T9/multi-tap TextEntry
 * pre-filled with the slot's current text. Confirm writes it back to
 * CannedService (empty text disables that button); cancel leaves it untouched.
 */
class CannedEditScreen : public LVScreen, private InputListener {
public:
	explicit CannedEditScreen(size_t slot);

	void onStart() override;
	void onStop() override;

private:
	void buttonPressed(uint i) override;

	void confirm();
	void cancel();

	size_t slot;
	TextEntry* textEntry;
};

#endif //CHATTER_FIRMWARE_CANNEDEDITSCREEN_H
