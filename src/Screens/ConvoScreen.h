#ifndef CHATTER_FIRMWARE_CONVOSCREEN_H
#define CHATTER_FIRMWARE_CONVOSCREEN_H

#include <Arduino.h>
#include <map>
#include <string>
#include "../Interface/LVScreen.h"
#include "../Elements/TextEntry.h"
#include "../Types.hpp"
#include "../Model/Profile.hpp"
#include "../Model/Convo.hpp"
#include "../Elements/ConvoBox.h"
#include "../Modals/ContextMenu.h"
#include "../Elements/PicMenu.h"

class ConvoScreen : public LVScreen, private InputListener{
public:
	ConvoScreen(UID_t uid);
	void onStart() override;
	void onStop() override;
	void onStarting() override;

private:
	void buttonPressed(uint i) override;
	Friend fren;
	const UID_t convo = 0;

	// Per-conversation unsent-message drafts, kept in memory across screen
	// instances so leaving mid-message and coming back later restores it
	// (cleared only once the message is actually sent -- see sendMessage()).
	static std::map<UID_t, std::string> drafts;

	void textEntryConfirm();
	void textEntryCancel();
	void textEntryLR();
	void convoBoxEnter();
	void convoBoxExit();
	void messageSelected(const Message& msg);
	void menuMessageSelected();
	void menuMessageCancel();
	void picMenuSelected();
	void picMenuCancel();

	void sendMessage();
	void buttonHeld(uint i) override;
	void buttonReleased(uint i) override;

	ConvoBox* convoBox;
	TextEntry* textEntry;
	PicMenu* picMenu;
	ContextMenu* menuMessage;

	Message selectedMessage;
};


#endif //CHATTER_FIRMWARE_CONVOSCREEN_H
