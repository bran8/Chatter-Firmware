#ifndef CHATTER_FIRMWARE_WEBUISERVICE_H
#define CHATTER_FIRMWARE_WEBUISERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
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
	// This AP has no internet uplink, which makes phone OSes (Android and iOS
	// both) treat it as broken: they probe a known URL to check for real
	// internet, the probe fails, and they either nag the user to confirm
	// "stay connected?" or silently stop routing app traffic over this Wi-Fi --
	// which looks exactly like "can't get back to the homepage" after a brief
	// disconnect/reconnect. Resolving every hostname to our own IP (below) lets
	// those probe requests land on handleCaptivePortal() instead of failing,
	// so the OS recognizes this as a normal captive-portal network and keeps
	// the connection usable.
	DNSServer dnsServer;
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
	void handleStatus();
	void handleGetProfile();
	void handleSetProfile();
	void handleAvatar();          // serves a built-in avatar as a BMP for the picker
	void handleSilence();         // stop an in-progress incoming-message alert
	void handleDeleteFriend();    // remove a friend + its conversation
	void handleDeleteMessage();   // remove one message from a conversation
	void handlePairStart();
	void handlePairDiscovered();
	void handlePairConfirm();
	void handlePairStatus();
	void handlePairCancel();
	void handleNotFound();
	void handleCaptivePortal();   // OS internet-check probes -> the portal page
	void redirectToPortal();      // 302 to the absolute gateway URL

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

	// --- Low-battery / backup-power alert ---------------------------------
	// There is no USB/charge-detect pin (only the battery ADC), so we can't sense
	// "USB just unplugged" directly. Instead we watch the battery percentage: on
	// USB power it stays high, so a drop past a threshold means the pack is
	// draining on backup. We chirp a distinctive falling cue once per downward
	// crossing (hysteresis avoids repeat nagging) and expose lowBattery in the
	// status JSON so the web page can show an "ON BATTERY" banner.
	void pollBattery();
	uint32_t batteryPollTimer = 0;
	bool lowBattery = false;               // <= warn threshold (shown on web)
	bool criticalBattery = false;          // <= critical threshold

	// --- Shutdown-on-battery ----------------------------------------------
	// When USB power is lost the unit runs on its backup pack; rather than
	// draining it forever serving an idle AP, power off after the configured
	// shutdown time (Settings shutdownTime) of inactivity. The timer is
	// deferred while on USB or while the web UI is being used; see pollBattery().
	void touchActivity();                  // reset the idle timer ("UI in use")
	uint32_t lastWebActivity = 0;          // millis() of last activity / USB-present poll
};

extern WebUIService WebUI;

#endif //CHATTER_FIRMWARE_WEBUISERVICE_H
