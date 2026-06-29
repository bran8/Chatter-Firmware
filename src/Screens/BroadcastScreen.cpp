#include "BroadcastScreen.h"
#include "../Fonts/font.h"
#include <Input/Input.h>
#include <Pins.hpp>

// See BroadcastScreen.h: inert stub on the headless-wifi branch (no keypad/LCD).
BroadcastScreen::BroadcastScreen() : LVScreen(), apop(this){
	lv_obj_set_style_pad_all(obj, 3, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t* title = lv_label_create(obj);
	lv_label_set_text(title, ">> Broadcast All");
	lv_obj_set_style_text_color(title, lv_color_white(), 0);
	lv_obj_set_style_text_font(title, &pixelbasic7, 0);
	lv_obj_set_align(title, LV_ALIGN_CENTER);
}

void BroadcastScreen::onStart(){
	Input::getInstance()->addListener(this);
}

void BroadcastScreen::onStop(){
	Input::getInstance()->removeListener(this);
}

void BroadcastScreen::buttonPressed(uint i){
	if(i == BTN_BACK) pop();
}
