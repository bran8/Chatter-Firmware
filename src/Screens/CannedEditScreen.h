#ifndef CHATTER_FIRMWARE_CANNEDEDITSCREEN_H
#define CHATTER_FIRMWARE_CANNEDEDITSCREEN_H

#include <Arduino.h>
#include <cstddef>
#include "../Interface/LVScreen.h"
#include <Input/InputListener.h>

/**
 * Single-slot editor for a canned message. NOTE: on the headless-wifi branch
 * keypad text entry (TextEntry/T9) was removed -- there's no LCD/keypad, and
 * canned messages are edited from the web UI. This screen never runs here;
 * it's an inert stub kept so its caller (CannedScreen) still compiles.
 */
class CannedEditScreen : public LVScreen, private InputListener {
public:
	explicit CannedEditScreen(size_t slot);

	void onStart() override;
	void onStop() override;

private:
	void buttonPressed(uint i) override;

	size_t slot;
};

#endif //CHATTER_FIRMWARE_CANNEDEDITSCREEN_H
