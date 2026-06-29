#ifndef CHATTER_FIRMWARE_BROADCASTSCREEN_H
#define CHATTER_FIRMWARE_BROADCASTSCREEN_H

#include <Arduino.h>
#include "../Interface/LVScreen.h"
#include <Input/InputListener.h>
#include "../AutoPop.h"

// Headless-wifi branch: keypad text entry (TextEntry/T9) was removed -- no
// LCD/keypad on this device, and broadcasting is done from the web UI. This
// screen never runs here; it's an inert stub kept so its caller (InboxScreen)
// still compiles.
class BroadcastScreen : public LVScreen, private InputListener {
public:
	BroadcastScreen();
	void onStart() override;
	void onStop() override;

private:
	void buttonPressed(uint i) override;

	AutoPop apop;
};

#endif //CHATTER_FIRMWARE_BROADCASTSCREEN_H
