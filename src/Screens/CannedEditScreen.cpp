#include "CannedEditScreen.h"
#include "../Fonts/font.h"
#include "../Services/CannedService.h"
#include <Input/Input.h>
#include <Pins.hpp>

CannedEditScreen::CannedEditScreen(size_t slot) : LVScreen(), slot(slot){
	lv_obj_set_style_pad_all(obj, 3, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t* container = lv_obj_create(obj);
	lv_obj_set_size(container, lv_pct(100), lv_pct(100));
	lv_obj_set_style_border_width(container, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(container, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
	lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);

	lv_obj_t* header = lv_obj_create(container);
	lv_obj_set_width(header, lv_pct(100));
	lv_obj_set_height(header, LV_SIZE_CONTENT);
	lv_obj_set_style_pad_all(header, 4, 0);
	lv_obj_set_style_border_width(header, 1, 0);
	lv_obj_set_style_border_color(header, lv_color_white(), 0);
	lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
	lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t* title = lv_label_create(header);
	lv_label_set_text_fmt(title, "Edit key [%s]", CannedService::keyLabel(slot));
	lv_obj_set_style_text_color(title, lv_color_white(), 0);
	lv_obj_set_style_text_font(title, &pixelbasic7, 0);
	lv_obj_set_align(title, LV_ALIGN_CENTER);

	lv_obj_t* spacer = lv_obj_create(container);
	lv_obj_set_width(spacer, lv_pct(100));
	lv_obj_set_flex_grow(spacer, 1);
	lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t* hint = lv_label_create(spacer);
	lv_label_set_text(hint, "Edit the message.\nLeave blank to clear.");
	lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(hint, lv_pct(100));
	lv_obj_set_style_text_color(hint, lv_color_white(), 0);
	lv_obj_set_style_text_font(hint, &pixelbasic7, 0);
	lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_align(hint, LV_ALIGN_CENTER);

	textEntry = new TextEntry(container, Canned.get(slot), 60);
	textEntry->showCaps(true);

	lv_obj_set_style_bg_opa(textEntry->getLvObj(), LV_OPA_100, LV_PART_MAIN);
	lv_obj_set_style_bg_color(textEntry->getLvObj(), lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_pad_hor(textEntry->getLvObj(), 2, 0);
	lv_obj_set_style_pad_top(textEntry->getLvObj(), 1, 0);
	lv_obj_set_style_text_font(textEntry->getLvObj(), &lv_font_montserrat_14, 0);
	textEntry->setTextColor(lv_color_black());

	lv_obj_add_event_cb(textEntry->getLvObj(), [](lv_event_t* e){
		static_cast<CannedEditScreen*>(e->user_data)->confirm();
	}, EV_ENTRY_DONE, this);

	lv_obj_add_event_cb(textEntry->getLvObj(), [](lv_event_t* e){
		static_cast<CannedEditScreen*>(e->user_data)->cancel();
	}, EV_ENTRY_CANCEL, this);
}

void CannedEditScreen::onStart(){
	Input::getInstance()->addListener(this);
	textEntry->start();
}

void CannedEditScreen::onStop(){
	Input::getInstance()->removeListener(this);
	textEntry->stop();
}

void CannedEditScreen::buttonPressed(uint i){
	if(i == BTN_ENTER || i == BTN_LEFT || i == BTN_RIGHT) return;

	if(i != BTN_BACK){
		if(textEntry->isActive()) return;
		textEntry->start();
		textEntry->keyPress(i);
		return;
	}

	if(textEntry->isActive()) return;
	pop();
}

void CannedEditScreen::confirm(){
	// TextEntry has already committed/stopped by the time EV_ENTRY_DONE fires, so
	// getText() returns the final string. Empty text disables the slot.
	Canned.set(slot, textEntry->getText());
	pop();
}

void CannedEditScreen::cancel(){
	pop();   // discard edits; TextEntry already stopped itself on cancel
}
