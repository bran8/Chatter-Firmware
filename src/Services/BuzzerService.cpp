#include "BuzzerService.h"
#include <Pins.hpp>
#include <Audio/Piezo.h>
#include <Notes.h>
#include <Loop/LoopManager.h>
#include <Input/Input.h>
#include <Settings.h>
#include <LITTLEFS.h>

#define KEYPAD_SOUND_PATH "/keypad_sound.txt"

BuzzerService Buzz;


void BuzzerService::begin(){
	loadKeypadSound();
	Messages.addReceivedListener(this);
	Input::getInstance()->addListener(this);
}

void BuzzerService::loadKeypadSound(){
	keypadSoundsEnabled = true;   // default ON when no file exists yet
	if(!LITTLEFS.exists(KEYPAD_SOUND_PATH)) return;

	File f = LITTLEFS.open(KEYPAD_SOUND_PATH, "r");
	if(!f) return;
	String s = f.readStringUntil('\n');
	f.close();
	s.trim();
	keypadSoundsEnabled = (s == "1" || s == "on" || s == "true");
}

void BuzzerService::setKeypadSounds(bool enabled){
	keypadSoundsEnabled = enabled;
	File f = LITTLEFS.open(KEYPAD_SOUND_PATH, "w");
	if(!f) return;
	f.print(enabled ? "1" : "0");
	f.close();
}

bool BuzzerService::getKeypadSounds() const{
	return keypadSoundsEnabled;
}

void BuzzerService::emitBeep(){
	if(!Settings.get().sound) return;
	if(!keypadSoundsEnabled) return;
	Piezo.tone(150, 100);   // 150 Hz for 100 ms
}

const std::unordered_map<uint8_t, uint16_t> BuzzerService::noteMap = {
		{BTN_1, NOTE_C4},
		{BTN_2, NOTE_D4},
		{BTN_3, NOTE_E4},
		{BTN_4, NOTE_F4},
		{BTN_5, NOTE_G4},
		{BTN_6, NOTE_A4},
		{BTN_7, NOTE_B4},
		{BTN_8, NOTE_C5},
		{BTN_9, NOTE_D5},
		{BTN_L, NOTE_E5},
		{BTN_0, NOTE_F5},
		{BTN_R, NOTE_G5},

		{BTN_LEFT, NOTE_B4},
		{BTN_RIGHT, NOTE_B4},
		{BTN_ENTER, NOTE_C5},
		{BTN_B, NOTE_A4}
};

const std::vector<BuzzerService::Note> BuzzerService::Notes = {
		{ NOTE_B5, 100000 },
		{ 0, 50000 },
		{ NOTE_B4, 100000 },
		{ NOTE_C5, 100000 },
		{ 0, 50000 },
		{ NOTE_G4, 100000 },
		{ 0, 100000 },
		{ NOTE_C5, 500000 }
};


void BuzzerService::msgReceived(const Message &message){
    if(!Settings.get().sound) return;
    //if(message.convo == noBuzzUID && noBuzzUID != ESP.getEfuseMac()) return;

    alertActive = true;

    LoopManager::defer([this](uint32_t){
        LoopManager::defer([this](uint32_t){
            noteIndex = 0;
            noteTime = 0;
            LoopManager::addListener(this);
            Piezo.tone(Notes[noteIndex].freq);
        });
    });
}

void BuzzerService::setNoBuzzUID(UID_t noBuzzUid){
	noBuzzUID = noBuzzUid;
}

void BuzzerService::silenceAlert(){
	Piezo.noTone();
	alertActive = false;
	noteIndex = 0;
	noteTime = 0;
	LoopManager::removeListener(this);
}

void BuzzerService::buttonPressed(uint i){
    extern bool gameStarted;
    if(gameStarted) return;
    if(!Settings.get().sound) return;

	// Keypad clicks are gated by their own setting; the alert-cancel bookkeeping
	// below must still run so any button press silences an incoming-message alert.
	if(keypadSoundsEnabled){
		// Playing the keypad note takes over the speaker, which already
		// interrupts any ongoing alert -- do NOT call noTone() here or the
		// keypad "music" would be cut off immediately.
		Piezo.tone(noteMap.at(i), 25);
	}else{
		// Keypad sounds are off, so no note replaces the alert tone; silence
		// the speaker explicitly to stop an in-progress incoming-message alert.
		Piezo.noTone();
	}
    alertActive = false;
    noteIndex = 0;
    noteTime = 0;
    LoopManager::removeListener(this);
	//printf("Alert canceled, %d pressed\n", i);
}

void BuzzerService::loop(uint micros){
    if(!Settings.get().sound){
        alertActive = false;
        Piezo.noTone();
        LoopManager::removeListener(this);
        return;
    }

    noteTime += micros;
    if(noteTime < Notes[noteIndex].duration) return;

    noteIndex++;
    noteTime = 0;

    if(noteIndex >= Notes.size()){
        if(alertActive){
            noteIndex = 0;
        }else{
            LoopManager::removeListener(this);
            Piezo.noTone();
            return;
        }
    }

    if(Notes[noteIndex].freq == 0){
        Piezo.noTone();
        return;
    }

    Piezo.tone(Notes[noteIndex].freq);
}

void BuzzerService::setMuteEnter(bool muteEnter){
	BuzzerService::muteEnter = muteEnter;
}


