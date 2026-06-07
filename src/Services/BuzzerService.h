#ifndef CHATTER_FIRMWARE_BUZZERSERVICE_H
#define CHATTER_FIRMWARE_BUZZERSERVICE_H

#include <Input/InputListener.h>
#include "MessageService.h"

class BuzzerService : public InputListener, private MsgReceivedListener, public LoopListener  {
public:
	void begin();
	void loop(uint micros) override;
	void setNoBuzzUID(UID_t noBuzzUid);

	void setMuteEnter(bool muteEnter);

	// Keypad sounds: persisted separately to /keypad_sound.txt on SPIFFS so the
	// toggle survives reboots without modifying the external Settings library.
	void setKeypadSounds(bool enabled);
	bool getKeypadSounds() const;

	// Short low error tone for invalid T9 keystrokes.
	void emitBeep();
private:
	void buttonPressed(uint i) override;
	void msgReceived(const Message &message) override;

	void loadKeypadSound();

	bool keypadSoundsEnabled = true;

	UID_t noBuzzUID = ESP.getEfuseMac();

	static const std::unordered_map<uint8_t, uint16_t> noteMap;
	struct Note { uint16_t freq; uint32_t duration; };
	static const std::vector<Note> Notes;

	uint32_t noteTime = 0;
	uint8_t noteIndex = 0;
	bool muteEnter = false;
	bool alertActive = false;
};

extern BuzzerService Buzz;
#endif //CHATTER_FIRMWARE_BUZZERSERVICE_H
