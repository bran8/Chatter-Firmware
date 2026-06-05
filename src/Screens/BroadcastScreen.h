#ifndef CHATTER_FIRMWARE_BROADCASTSCREEN_H
#define CHATTER_FIRMWARE_BROADCASTSCREEN_H

#include <Arduino.h>
#include "../Interface/LVScreen.h"
#include "../Elements/TextEntry.h"
#include "../AutoPop.h"

class BroadcastScreen : public LVScreen, private InputListener {
public:
	BroadcastScreen();
	void onStart() override;
	void onStop() override;

private:
	void buttonPressed(uint i) override;
	void buttonHeld(uint i) override;

	void textEntryConfirm();
	void textEntryCancel();

	void sendBroadcast();

	TextEntry* textEntry;
	AutoPop apop;
};

#endif //CHATTER_FIRMWARE_BROADCASTSCREEN_H
