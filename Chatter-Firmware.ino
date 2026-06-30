#include <Arduino.h>
#include <CircuitOS.h>
#include <Chatter.h>
#include <Loop/LoopManager.h>
#include <lvgl.h>
#include "src/InputChatter.h"
#include "src/FSLVGL.h"
#include "src/ChatterTheme.h"
#include <SPIFFS.h>
#include "src/Screens/MainMenu.h"
#include "src/Storage/Storage.h"
#include "src/Services/LoRaService.h"
#include "src/Services/MessageService.h"
#include "src/Services/CustomDictService.h"
#include "src/Services/CannedService.h"
#include "src/Elements/IntroScreen.h"
#include "src/Interface/Pics.h"
#include "src/Services/ProfileService.h"
#include "src/Screens/UserHWTest.h"
#include <Settings.h>
#include <Util/HWRevision.h>
#include "src/Services/SleepService.h"
#include "src/Services/ShutdownService.h"
#include "src/Services/BuzzerService.h"
#include "src/JigHWTest/JigHWTest.h"
#include "src/Games/GameEngine/Game.h"

lv_disp_draw_buf_t drawBuffer;
Display* display;

void lvglFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p){
	uint32_t w = (area->x2 - area->x1 + 1);
	uint32_t h = (area->y2 - area->y1 + 1);

	TFT_eSPI &tft = *display->getTft();
	tft.startWrite();
	tft.setAddrWindow(area->x1, area->y1, w, h);
	tft.pushColors(&color_p->full, w * h, true);
	tft.endWrite();
	lv_disp_flush_ready(disp);
}

struct {
	UID_t uid;
	const char* nickname;
	uint8_t avatar;
	uint16_t hue;
} const Chatters[] = {
		{ 0xdcba4bf23a08, "Grle v0.5", 1, 0 },
		{ 0x48275612cfa4, "Choki v0.6", 2, 80 },
		{ 0x88a61312cfa4, "Zeus v0.6", 5, 90 },
		{ 0x48e4e0e6e2e0, "Johnny v0.4", 3, 160 },
		{ 0xe4c14bf23a08, "Rus v0.5", 4, 240 },
		{ 0x38c24bf23a08, "Crni v0.5", 6, 40 },
};

void loadMock(bool clear = false){
	if(clear){
		for(UID_t uid : Storage.Messages.all()) Storage.Messages.remove(uid);
		for(UID_t uid : Storage.Friends.all()) Storage.Friends.remove(uid);
		for(UID_t uid : Storage.Convos.all()) Storage.Convos.remove(uid);
	}

	for(const auto& chatter : Chatters){
		Friend fren;
		fren.uid = chatter.uid;
		fren.profile.avatar = chatter.avatar;
		fren.profile.hue = (uint8_t) (chatter.hue / 2);
		strncpy(fren.profile.nickname, chatter.nickname, 15);
		Storage.Friends.add(fren);

		if(chatter.uid == ESP.getEfuseMac()) continue;

		Convo convo;
		convo.uid = fren.uid;

		const int count = LoRa.rand() % 5;
		for(int i = 0; i < count; i++){
			Message message;
			if(LoRa.rand() % 2){
				message.setText("Hello!");
			}else{
				message.setPic(LoRa.rand() % NUM_PICS);
			}

			message.uid = LoRa.randUID();
			message.received = LoRa.rand() % 2;
			message.outgoing = LoRa.rand() % 2;
			convo.messages.push_back(message.uid);
			Storage.Messages.add(message);
		}

		Storage.Convos.add(convo);
	}

	// Reinit my profile
	Profiles.begin();
}

void printData(){
	for(UID_t uid : Storage.Friends.all()){
		Friend fren = Storage.Friends.get(uid);
		printf("%llx | Fren: %s | Hue: %d | Avatar: %d\n", fren.uid, fren.profile.nickname, fren.profile.hue, fren.profile.avatar);

		Convo convo = Storage.Convos.get(uid);
		printf("%llx | Convo: %d messages\n", convo.uid, convo.messages.size());

		for(UID_t uid : convo.messages){
			Message message = Storage.Messages.get(uid);
			printf("%llx | Message: %s\n", message.uid, message.getText().c_str());
		}
	}
}

void boot(){
	printf("[BOOT] boot() entered — drawing splash\n"); Serial.flush();
	lv_timer_handler();

	display->getBaseSprite()->drawIcon(SPIFFS.open("/splash.raw"), 0, 0, 160, 128);
	display->commit();
	printf("[BOOT] splash committed\n"); Serial.flush();

	Chatter.fadeIn();

	FSLVGL::loadCache();
	printf("[BOOT] FSLVGL cache loaded\n"); Serial.flush();

	Storage.begin();      printf("[BOOT] Storage.begin() ok\n"); Serial.flush();
	CustomDict.begin();   // load learned words before any message is processed
	printf("[BOOT] CustomDict.begin() ok\n"); Serial.flush();
	Canned.begin();       // load persisted canned messages (seeds defaults first boot)
	printf("[BOOT] Canned.begin() ok\n"); Serial.flush();
	Messages.begin();     printf("[BOOT] Messages.begin() ok\n"); Serial.flush();

	LoRa.begin();         printf("[BOOT] LoRa.begin() ok (915 MHz)\n"); Serial.flush();
	Profiles.begin();     printf("[BOOT] Profiles.begin() ok\n"); Serial.flush();

	//loadMock(true);
	//printData();

	// Set default sleep time to 5 minutes if it's at the library default (0)
	if(Settings.get().sleepTime == 0) {
		Settings.get().sleepTime = 2;  // Index 2 = 5 minutes
		Settings.store();
	}

	Sleep.begin();        printf("[BOOT] Sleep.begin() ok\n"); Serial.flush();


	auto intro = new IntroScreen([](){
		Shutdown.begin();
		Buzz.begin();
	});

	lv_timer_handler();

	intro->start();
	printf("[BOOT] IntroScreen started — handing off to main loop\n"); Serial.flush();
}

bool checkJig(){
	char buf[7];
	int wp = 0;

	uint32_t start = millis();
	int c;
	while(millis() - start < 500){
		vTaskDelay(1);
		c = getchar();
		if(c == EOF) continue;
		buf[wp] = (char) c;
		wp = (wp + 1) % 7;

		for(int i = 0; i < 7; i++){
			int match = 0;
			static const char* target = "JIGTEST";

			for(int j = 0; j < 7; j++){
				match += buf[(i + j) % 7] == target[j];
			}

			if(match == 7) return true;
		}
	}

	return false;
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
	delay(300);   // give the USB serial monitor time to attach so early lines aren't lost

	printf("\n\n[BOOT] ===== POST: setup() entered =====\n");
	printf("[BOOT] Chip UID: 0x%llx | HWRevision: %d\n", ESP.getEfuseMac(), HWRevision::get());
	printf("[BOOT] BATTERY_PIN = GPIO %d\n", BATTERY_PIN);
	Serial.flush();

	randomSeed(analogRead(BATTERY_PIN) * 13 + analogRead(BATTERY_PIN) * 7 + 2);

	LoopManager::reserve(24);

	printf("[BOOT] Chatter.begin() ...\n"); Serial.flush();
	Chatter.begin(false);
	display = Chatter.getDisplay();
	printf("[BOOT] Chatter.begin() done | display=%p\n", (void*) display); Serial.flush();

	initLog();

	printf("[BOOT] checkJig() (500ms window for 'JIGTEST') ...\n"); Serial.flush();
	if(checkJig()){
		printf("[BOOT] JIG MODE entered — running JigHWTest, will not boot normally\n"); Serial.flush();
		auto test = new JigHWTest(display);
		Chatter.fadeIn();
		test->start();
		for(;;);
	}else{
		printf("[BOOT] No jig — continuing normal boot\n");
	}

	// Dead-battery guard. Reads BATTERY_PIN (GPIO 36 on this rev-1 board).
	// If running on USB with no/flat battery, this reads ~0% and turns the device
	// off BEFORE the splash is ever drawn -> totally blank screen.
	uint8_t battPct = Battery.getPercentage();
	uint16_t battMv = Battery.getVoltage();
	printf("[BOOT] Battery check: %u%% (%u mV) on GPIO %d\n", battPct, battMv, BATTERY_PIN); Serial.flush();
	if(battPct == 0){
		printf("[BOOT] *** DEAD-BATTERY GUARD FIRING -> Sleep.turnOff() -> blank screen ***\n");
		printf("[BOOT] *** If the device is on USB power, this is a false trip. ***\n");
		Serial.flush();
		LoRa.initStateless();
		Sleep.turnOff();
		for(;;);
	}

	Piezo.setMute(!Settings.get().sound);

	lv_init();
	lv_disp_draw_buf_init(&drawBuffer, display->getBaseSprite()->getBuffer(), NULL, 160 * 128);

	new FSLVGL(SPIFFS, 'S');

	static lv_disp_drv_t displayDriver;
	lv_disp_drv_init(&displayDriver);
	displayDriver.hor_res = 160;
	displayDriver.ver_res = 128;
	displayDriver.flush_cb = lvglFlush;
	displayDriver.draw_buf = &drawBuffer;
	displayDriver.full_refresh = true;
	lv_disp_t * disp = lv_disp_drv_register(&displayDriver);
	chatterThemeInit(disp);

	Chatter.getInput()->addListener(new InputChatter());

	printf("UID: 0x%llx\n", ESP.getEfuseMac());

	if(!Settings.get().tested){
		if(HWRevision::get() > 0){
			Settings.get().tested = true;
			Settings.store();
		}else{
			FSLVGL::loadCache();

			auto test = new UserHWTest([](){
				Settings.get().tested = true;
				Settings.store();
				Chatter.fadeOut();
				boot();
			});

			test->start();
			lv_timer_handler();
			Chatter.fadeIn();
			return;
		}
	}

	boot();
}

bool gameStarted = false;
Game* startedGame = nullptr;

void loop(){
	if(!gameStarted){
		lv_timer_handler();
	}
	LoopManager::loop();
}

