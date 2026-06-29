#include "ConvoScreen.h"
#include <Input/Input.h>
#include <Pins.hpp>
#include "../Fonts/font.h"

// See ConvoScreen.h: inert stub on the headless-wifi branch (no keypad/LCD).
ConvoScreen::ConvoScreen(UID_t uid) : convo(uid){
	lv_obj_set_style_pad_all(obj, 3, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t* label = lv_label_create(obj);
	lv_label_set_text(label, "Conversations are\nhandled over Wi-Fi");
	lv_obj_set_style_text_color(label, lv_color_white(), 0);
	lv_obj_set_style_text_font(label, &pixelbasic7, 0);
	lv_obj_set_align(label, LV_ALIGN_CENTER);
}

void ConvoScreen::onStart(){
	Input::getInstance()->addListener(this);
}

void ConvoScreen::onStop(){
	Input::getInstance()->removeListener(this);
}

void ConvoScreen::buttonPressed(uint i){
	if(i == BTN_BACK) pop();
}
