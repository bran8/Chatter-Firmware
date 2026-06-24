#include "CannedService.h"
#include <Arduino.h>
#include <SPIFFS.h>

#define CANNED_PATH "/canned.txt"

CannedService Canned;

// Slot order mirrors the keypad: index 0..8 -> keys 1..9, index 9 -> key 0.
static const char* kDefaults[CannedService::Count] = {
	"Stop for a bathroom break!",    // 1
	"Radio silence for 10 minutes",  // 2
	"On our way",                    // 3
	"Call me when you can",          // 4
	"Arrived at destination!",       // 5
	"Be there in 5 minutes",         // 6
	"We need food!",                 // 7
	"Traffic is bad",                // 8
	"Confirmed",                     // 9
	"All is good!",                  // 0
};

static const char* kLabels[CannedService::Count] = {
	"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"
};

void CannedService::begin(){
	if(SPIFFS.exists(CANNED_PATH)){
		load();
	}else{
		resetDefaults();   // seeds slots and writes the file
	}
}

const std::string& CannedService::get(size_t slot) const{
	static const std::string empty;
	if(slot >= Count) return empty;
	return slots[slot];
}

void CannedService::set(size_t slot, const std::string& text){
	if(slot >= Count) return;
	slots[slot] = text;
	save();
}

void CannedService::resetDefaults(){
	for(size_t i = 0; i < Count; i++) slots[i] = kDefaults[i];
	save();
}

const char* CannedService::defaultText(size_t slot){
	return slot < Count ? kDefaults[slot] : "";
}

const char* CannedService::keyLabel(size_t slot){
	return slot < Count ? kLabels[slot] : "";
}

void CannedService::load(){
	File f = SPIFFS.open(CANNED_PATH, "r");
	if(!f){ resetDefaults(); return; }

	// One line per slot, in slot order. A canned message is always single-line
	// (the editor is one-line), so newline is a safe record delimiter. A blank
	// line leaves the slot disabled.
	for(size_t i = 0; i < Count; i++){
		if(!f.available()){ slots[i].clear(); continue; }
		String line = f.readStringUntil('\n');
		while(line.length() && line[line.length() - 1] == '\r'){
			line.remove(line.length() - 1);
		}
		slots[i] = std::string(line.c_str());
	}
	f.close();
}

void CannedService::save(){
	File f = SPIFFS.open(CANNED_PATH, "w");
	if(!f) return;
	for(size_t i = 0; i < Count; i++){
		f.print(slots[i].c_str());
		f.print('\n');
	}
	f.close();
}
