#include "BroadcastScreen.h"
#include "../Fonts/font.h"
#include "../Services/MessageService.h"
#include <Input/Input.h>
#include <Pins.hpp>

/**
 * @file BroadcastScreen.cpp
 * @brief Specialized screen for one-to-many rapid communication.
 * 
 * OVERVIEW:
 * This screen is optimized for sending quick status updates to all friends list.  Canned messaged will be moved and centralized soon.
 * 
 * KEY FEATURES:
 * - One-to-Many: Uses Messages.broadcastText() to reach all peers at once.
 * - Canned Messages: Predefined phrases mapped to physical buttons for instant broadcasting.
 * - Rapid Input: Integration with TextEntry for custom messages.
 * 
 * WORKFLOW:
 * 1. User enters screen via "[>> Broadcast All]" menu.
 * 2. User either types a custom message OR presses a physical button (BTN_0-9) for a canned message.
 * 3. Pressing BTN_ENTER (or EV_ENTRY_DONE) sends the message and automatically pops the screen.
 * 4. BTN_BACK pops the screen if not currently typing.
 */

BroadcastScreen::BroadcastScreen() : LVScreen(), apop(this){
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
	lv_label_set_text(title, ">> Broadcast All");
	lv_obj_set_style_text_color(title, lv_color_white(), 0);
	lv_obj_set_style_text_font(title, &pixelbasic7, 0);
	lv_obj_set_align(title, LV_ALIGN_CENTER);

	lv_obj_t* spacer = lv_obj_create(container);
	lv_obj_set_width(spacer, lv_pct(100));
	lv_obj_set_flex_grow(spacer, 1);
	lv_obj_set_scrollbar_mode(spacer, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t* hint = lv_label_create(spacer);
	lv_label_set_text(hint, "Type a message to\nsend to all friends");
	lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(hint, lv_pct(100));
	lv_obj_set_style_text_color(hint, lv_color_white(), 0);
	lv_obj_set_style_text_font(hint, &pixelbasic7, 0);
	lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_align(hint, LV_ALIGN_CENTER);

	textEntry = new TextEntry(container, "", 60);
	textEntry->showCaps(true);

	textEntry->setCannedMessage(BTN_1, "Stop for a bathroom break!");
	textEntry->setCannedMessage(BTN_2, "Radio silence for 10 minutes");
	textEntry->setCannedMessage(BTN_3, "On our way");
	textEntry->setCannedMessage(BTN_4, "Call me when you can");
	textEntry->setCannedMessage(BTN_5, "Arrived at destination!");
	textEntry->setCannedMessage(BTN_6, "Be there in 5 minutes");
	textEntry->setCannedMessage(BTN_7, "We need food!");
	textEntry->setCannedMessage(BTN_8, "Traffic is bad");
	textEntry->setCannedMessage(BTN_9, "Confirmed");
	textEntry->setCannedMessage(BTN_0, "All is good!");

	lv_obj_set_style_bg_opa(textEntry->getLvObj(), LV_OPA_100, LV_PART_MAIN);
	lv_obj_set_style_bg_color(textEntry->getLvObj(), lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_pad_hor(textEntry->getLvObj(), 2, 0);
	lv_obj_set_style_pad_top(textEntry->getLvObj(), 1, 0);
	lv_obj_set_style_text_font(textEntry->getLvObj(), &lv_font_montserrat_14, 0);
	textEntry->setTextColor(lv_color_black());

	lv_obj_add_event_cb(textEntry->getLvObj(), [](lv_event_t* e){
		static_cast<BroadcastScreen*>(e->user_data)->textEntryConfirm();
	}, EV_ENTRY_DONE, this);

	lv_obj_add_event_cb(textEntry->getLvObj(), [](lv_event_t* e){
		static_cast<BroadcastScreen*>(e->user_data)->textEntryCancel();
	}, EV_ENTRY_CANCEL, this);
}

void BroadcastScreen::onStart(){
	Input::getInstance()->addListener(this);
	textEntry->start();
}

void BroadcastScreen::onStop(){
	Input::getInstance()->removeListener(this);
	textEntry->stop();
}

void BroadcastScreen::buttonPressed(uint i){
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

void BroadcastScreen::buttonHeld(uint i){
}

void BroadcastScreen::sendBroadcast(){
	std::string text = textEntry->getText();
	if(text.empty()) return;

	textEntry->clear();

	int sent = Messages.broadcastText(text);
	printf("Broadcast sent to %d friends\n", sent);

	pop();
}

void BroadcastScreen::textEntryConfirm(){
	sendBroadcast();
}

void BroadcastScreen::textEntryCancel(){
	textEntry->stop();

	if(textEntry->getText().empty()){
		pop();
		return;
	}
}
