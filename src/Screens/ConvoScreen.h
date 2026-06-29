#ifndef CHATTER_FIRMWARE_CONVOSCREEN_H
#define CHATTER_FIRMWARE_CONVOSCREEN_H

#include <Arduino.h>
#include "../Interface/LVScreen.h"
#include "../Types.hpp"
#include <Input/InputListener.h>

// Headless-wifi branch: keypad text entry (TextEntry/T9) was removed -- this
// device has no LCD/keypad, and conversations are driven entirely from the web
// UI (see WebUIService). This screen never runs here; it's an inert stub kept
// only so its caller (InboxScreen) still compiles.
class ConvoScreen : public LVScreen, private InputListener{
public:
	ConvoScreen(UID_t uid);
	void onStart() override;
	void onStop() override;

private:
	void buttonPressed(uint i) override;
	const UID_t convo = 0;
};

#endif //CHATTER_FIRMWARE_CONVOSCREEN_H
