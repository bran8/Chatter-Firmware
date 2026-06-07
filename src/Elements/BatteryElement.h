#ifndef CHATTER_FIRMWARE_BATTERYELEMENT_H
#define CHATTER_FIRMWARE_BATTERYELEMENT_H

#include <Arduino.h>
#include "../Interface/LVObject.h"
#include <Chatter.h>

class BatteryElement : public LVObject, public LoopListener {
public:
	BatteryElement(lv_obj_t* parent);

	virtual ~BatteryElement();

	void loop(uint micros) override;

private:
	void updateLabel();

	lv_obj_t* label;
	uint16_t voltage;   // last reading in millivolts
};


#endif //CHATTER_FIRMWARE_BATTERYELEMENT_H
