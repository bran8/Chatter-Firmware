#include "BatteryElement.h"
#include "../Fonts/font.h"

BatteryElement::BatteryElement(lv_obj_t* parent) : LVObject(parent){
	lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

	label = lv_label_create(obj);
	// Smaller font than the default label, nudged right so it isn't flush
	// against the edge of its container.
	lv_obj_set_style_text_font(label, &pixelbasic7, 0);
	lv_obj_set_style_text_color(label, lv_color_white(), 0);
	lv_obj_set_style_pad_left(label, 4, 0);

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
