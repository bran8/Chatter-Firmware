#include "CannedScreen.h"
#include "CannedEditScreen.h"
#include "../Fonts/font.h"
#include "../Modals/Prompt.h"
#include <cstdint>

CannedScreen::CannedScreen() : LVScreen(){
	lv_style_selector_t sel = LV_PART_MAIN | LV_STATE_DEFAULT;
	lv_style_selector_t selFocus = LV_PART_MAIN | LV_STATE_FOCUSED;

	lv_style_init(&style_def);
	lv_style_set_border_width(&style_def, 1);
	lv_style_set_border_opa(&style_def, 0);

	lv_style_init(&style_focused);
	lv_style_set_border_width(&style_focused, 1);
	lv_style_set_border_color(&style_focused, lv_color_white());
	lv_style_set_border_opa(&style_focused, LV_OPA_COVER);

	lv_obj_set_size(obj, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
	lv_obj_set_style_pad_gap(obj, 2, 0);
	lv_obj_set_style_pad_all(obj, 3, 0);

	lv_obj_t* title = lv_label_create(obj);
	lv_obj_set_style_text_font(title, &pixelbasic7, 0);
	lv_obj_set_style_text_color(title, lv_color_white(), 0);
	lv_label_set_text(title, "Canned messages");

	// One focusable row per slot. The slot index rides in the row's user data so
	// a single shared callback can open the right editor.
	for(size_t i = 0; i < CannedService::Count; i++){
		lv_obj_t* row = lv_obj_create(obj);
		lv_obj_set_height(row, LV_SIZE_CONTENT);
		lv_obj_set_width(row, lv_pct(100));
		lv_obj_set_layout(row, LV_LAYOUT_FLEX);
		lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_all(row, 3, 0);
		lv_obj_set_style_bg_opa(row, 0, 0);
		lv_obj_add_style(row, &style_focused, selFocus);
		lv_obj_add_style(row, &style_def, sel);
		lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
		lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICK_FOCUSABLE);
		lv_obj_clear_flag(row, LV_OBJ_FLAG_CHECKABLE);
		lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_set_user_data(row, (void*) (intptr_t) i);

		lv_obj_t* label = lv_label_create(row);
		lv_obj_set_style_text_font(label, &pixelbasic7, 0);
		lv_obj_set_style_text_color(label, lv_color_white(), 0);
		lv_obj_set_width(label, lv_pct(100));
		lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
		rowLabels[i] = label;

		lv_obj_add_event_cb(row, [](lv_event_t* e){
			auto* screen = static_cast<CannedScreen*>(e->user_data);
			size_t slot = (size_t) (intptr_t) lv_obj_get_user_data(lv_event_get_target(e));
			screen->push(new CannedEditScreen(slot));
		}, LV_EVENT_CLICKED, this);

		lv_obj_add_event_cb(row, [](lv_event_t* e){
			static_cast<CannedScreen*>(e->user_data)->pop();
		}, LV_EVENT_CANCEL, this);

		lv_group_add_obj(inputGroup, row);
	}

	// Reset-to-defaults action.
	resetRow = lv_obj_create(obj);
	lv_obj_set_height(resetRow, LV_SIZE_CONTENT);
	lv_obj_set_width(resetRow, lv_pct(100));
	lv_obj_set_layout(resetRow, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(resetRow, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(resetRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_all(resetRow, 3, 0);
	lv_obj_set_style_bg_opa(resetRow, 0, 0);
	lv_obj_add_style(resetRow, &style_focused, selFocus);
	lv_obj_add_style(resetRow, &style_def, sel);
	lv_obj_add_flag(resetRow, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
	lv_obj_clear_flag(resetRow, LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_obj_clear_flag(resetRow, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_clear_flag(resetRow, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t* resetLabel = lv_label_create(resetRow);
	lv_obj_set_style_text_font(resetLabel, &pixelbasic7, 0);
	lv_obj_set_style_text_color(resetLabel, lv_color_white(), 0);
	lv_label_set_text(resetLabel, "Reset to defaults");

	lv_obj_add_event_cb(resetRow, [](lv_event_t* e){
		auto* screen = static_cast<CannedScreen*>(e->user_data);
		auto prompt = new Prompt(screen, "Reset all canned\nmessages to defaults?");
		lv_obj_add_event_cb(prompt->getLvObj(), [](lv_event_t* ev){
			auto* screen = static_cast<CannedScreen*>(ev->user_data);
			Canned.resetDefaults();
			screen->refresh();
		}, EV_PROMPT_YES, screen);
		prompt->start();
	}, LV_EVENT_CLICKED, this);

	lv_obj_add_event_cb(resetRow, [](lv_event_t* e){
		static_cast<CannedScreen*>(e->user_data)->pop();
	}, LV_EVENT_CANCEL, this);

	lv_group_add_obj(inputGroup, resetRow);
}

CannedScreen::~CannedScreen(){
	lv_style_reset(&style_def);
	lv_style_reset(&style_focused);
}

void CannedScreen::refresh(){
	for(size_t i = 0; i < CannedService::Count; i++){
		const std::string& t = Canned.get(i);
		if(t.empty()){
			lv_label_set_text_fmt(rowLabels[i], "[%s] --", CannedService::keyLabel(i));
		}else{
			lv_label_set_text_fmt(rowLabels[i], "[%s] %s", CannedService::keyLabel(i), t.c_str());
		}
	}
}

void CannedScreen::onStarting(){
	refresh();
}
