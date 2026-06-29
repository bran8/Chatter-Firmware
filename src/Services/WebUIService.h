#ifndef CHATTER_FIRMWARE_WEBUISERVICE_H
#define CHATTER_FIRMWARE_WEBUISERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Loop/LoopListener.h>
#include "Pair/PairService.h"

// Headless-device control surface: brings up a WiFi Access Point and a small
// HTTP API + embedded web page so the device can be operated from a phone
// browser instead of the (broken/absent) LCD + keypad. Wraps the existing
// Storage/MessageService/ProfileService/PairService APIs; doesn't duplicate
// any messaging logic.
class WebUIService : public LoopListener {
public:
	void begin();
	void loop(uint micros) override;

	// One note of a buzzer melody: frequency in Hz, length in ms. Public so the
	// cue tables can live at file scope in the .cpp.
	struct CueNote { uint16_t freq; uint16_t durMs; };

private:
	WebServer server{80};
	PairService* pairService = nullptr;
	bool pairResultReady = false;
	bool pairResultSuccess = false;

	void handleRoot();
	void handleFriends();
	void handleConvos();
	void handleConvo();
	void handleSendMessage();
	void handleBroadcast();
	void handleMarkRead();
	void handlePending();
	void handleGetProfile();
	void handleSetProfile();
	void handlePairStart();
	void handlePairDiscovered();
	void handlePairConfirm();
	void handlePairStatus();
	void handlePairCancel();
	void handleNotFound();

	static void onPairDone(bool success, void* ctx);

	static String jsonEscape(const String& s);
	static String uidToHex(UID_t uid);
	static UID_t hexToUid(const String& s);

	// --- Audio feedback ---------------------------------------------------
	// With no LCD, the buzzer is the only "this is alive / something changed"
	// indicator. We watch the SoftAP client count each loop and chirp a short
	// melody when a phone joins or leaves, plus a boot chime when the AP is up.
	// Tones honor the global sound setting via Piezo's mute (set in setup()).
	void playCue(const CueNote* notes, uint8_t len);
	void updateCue(uint micros);          // non-blocking note sequencer
	void pollStations();                  // connect/disconnect detection

	uint8_t lastStationNum = 0;
	uint32_t stationPollTimer = 0;         // micros since last station-count check

	const CueNote* activeCue = nullptr;
	uint8_t activeCueLen = 0;
	uint8_t cueIndex = 0;
	uint32_t cueTimer = 0;                 // micros accumulated in current note slot
};

extern WebUIService WebUI;

#endif //CHATTER_FIRMWARE_WEBUISERVICE_H
