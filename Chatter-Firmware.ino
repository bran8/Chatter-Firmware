// Headless build for the broken-LCD unit: no LVGL/display, no keypad UI.
// Boots straight into Storage/LoRa/Message/Profile services + a WiFi AP and
// web control panel (src/Services/WebUIService.h). Lives only on the
// headless-wifi branch -- master (and the other devices) are untouched.
#include <Arduino.h>
#include <CircuitOS.h>
#include <Chatter.h>
#include <Loop/LoopManager.h>
#include <LITTLEFS.h>
#include "src/Storage/Storage.h"
#include "src/Services/LoRaService.h"
#include "src/Services/MessageService.h"
#include "src/Services/CustomDictService.h"
#include "src/Services/CannedService.h"
#include "src/Services/ProfileService.h"
#include <Settings.h>
#include "src/Services/SleepService.h"
#include "src/Services/BuzzerService.h"
#include "src/Services/WebUIService.h"
#include "src/Games/GameEngine/Game.h"

// SleepService/ShutdownService (light/deep sleep, low-battery screens) are
// LVGL/Screen-coupled (LVScreen::getCurrent(), BatteryNotification, etc.) and
// would crash with no screen ever created -- intentionally not started here.
// That also suits this build: WiFi AP + web server need to stay up
// continuously, which sleep would interrupt anyway.
//
// SleepService.cpp/ShutdownService.cpp still compile as part of src/ and
// reference these two globals (used to gate game-related behavior on the
// other devices); keep them defined so the build links. They stay false/null.
bool gameStarted = false;
Game* startedGame = nullptr;

void boot(){
	Storage.begin();
	CustomDict.begin();   // load learned words before any message is processed
	Canned.begin();       // load persisted canned messages (seeds defaults first boot)
	Messages.begin();

	LoRa.begin();
	Profiles.begin();

	Buzz.begin();

	WebUI.begin();
}

void initLog(){
	esp_log_level_set("*", ESP_LOG_NONE);
	return;

	const static auto tags = { "*" };

	for(const char* tag : tags){
		esp_log_level_set(tag, ESP_LOG_VERBOSE);
	}
}

void setup(){
	Serial.begin(115200);

	randomSeed(analogRead(BATTERY_PIN) * 13 + analogRead(BATTERY_PIN) * 7 + 2);

	LoopManager::reserve(24);

	Chatter.begin(false);

	// formatOnFail=false: if the mount fails we want to SEE it on serial, not
	// silently format a blank FS (which wipes uploaded assets + the profile and
	// makes every boot look "fresh"). A mount failure almost always means the
	// firmware's partition scheme doesn't match where littlefs.bin was flashed
	// (No OTA = spiffs @ 0x211000; default = spiffs @ 0x291000).
	if(!LITTLEFS.begin(false)){
		printf("LittleFS mount FAILED -- check partition scheme matches the "
			   "littlefs.bin flash address (No OTA => 0x211000).\n");
	}else{
		printf("LittleFS mounted: %u / %u bytes used\n",
			   (unsigned)LITTLEFS.usedBytes(), (unsigned)LITTLEFS.totalBytes());
	}

	initLog();
	printf("\n");

	if(Battery.getPercentage() == 0){
		LoRa.initStateless();
		Sleep.turnOff();
		for(;;);
	}

	Piezo.setMute(!Settings.get().sound);

	printf("UID: 0x%llx\n", ESP.getEfuseMac());

	boot();
}

void loop(){
	LoopManager::loop();
}

