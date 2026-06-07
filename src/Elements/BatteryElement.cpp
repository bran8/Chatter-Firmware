#include "BatteryElement.h"

BatteryElement::BatteryElement(lv_obj_t* parent) : LVObject(parent){
	lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

	label = lv_label_create(obj);

	voltage = Battery.getVoltage();
	updateLabel();

	LoopManager::addListener(this);
}

void BatteryElement::updateLabel(){
	// Battery.getVoltage() is in millivolts (e.g. 4100 -> "4.1V").
	uint16_t whole = voltage / 1000;
	uint16_t tenths = (voltage % 1000) / 100;
	lv_label_set_text_fmt(label, "%u.%uV", whole, tenths);
}

void BatteryElement::loop(uint micros){
	uint16_t v = Battery.getVoltage();
	// Only redraw when the displayed tenths-of-a-volt actually changes.
	if(v / 100 != voltage / 100){
		voltage = v;
		updateLabel();
	}
}

BatteryElement::~BatteryElement(){
	LoopManager::removeListener(this);
}
