#include "TextEntry.h"
#include "T9Dict.h"
#include "../InputLVGL.h"
#include "../Fonts/font.h"
#include "../Services/CustomDictService.h"
#include "../Services/CannedService.h"
#include "../Services/BuzzerService.h"
#include <Input/Input.h>
#include <Pins.hpp>
#include <Loop/LoopManager.h>
#include <unordered_set>
#include <algorithm>
#include <cstring>

const char* TextEntry::characters[] = {
		".,?!+-:()*1",
		"abc2",
		"def3",
		"ghi4",
		"jkl5",
		"mno6",
		"pqrs7",
		"tuv8",
		"wxyz9",
		" 0"
};

// Punctuation cycled by BTN_1 while in T9 mode. Exclamation/question lead
// (most urgently needed -- "the typer wants an expletive, not a comma"),
// followed by the rest of the multi-tap set (mirrors characters[0]).
static const char* T9_PUNCT = "!?.,+-:()*1";

char* TextEntry::charMap = nullptr;

const std::map<uint8_t, uint8_t> TextEntry::keyMap = {
		{ BTN_1, 0 },
		{ BTN_2, 1 },
		{ BTN_3, 2 },
		{ BTN_4, 3 },
		{ BTN_5, 4 },
		{ BTN_6, 5 },
		{ BTN_7, 6 },
		{ BTN_8, 7 },
		{ BTN_9, 8 },
		{ BTN_0, 9 },
};

TextEntry::TextEntry(lv_obj_t* parent, const std::string& text, uint32_t maxLength) : LVObject(parent){
	this->maxLength = maxLength;

	lv_obj_set_size(obj, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

	entry = lv_textarea_create(obj);
	lv_obj_clear_flag(entry, LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_obj_clear_flag(entry, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_clear_flag(entry, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_flex_grow(entry, 1);
	lv_textarea_set_one_line(entry, true);
	lv_textarea_set_text(entry, text.c_str());
	lv_textarea_set_max_length(entry, maxLength);

	// Render the grey unconfirmed prediction via inline recolour markup.
	lv_label_set_recolor(lv_textarea_get_label(entry), true);

	lv_obj_set_style_border_width(entry, 1, 0);
	lv_obj_set_style_border_opa(entry, LV_OPA_0, 0);

	if(charMap == nullptr){
		std::unordered_set<char> set;

		for(auto chars : characters){
			size_t n = strnlen(chars, 15);

			for(int i = 0; i < n; i++){
				set.insert(toLowerCase(chars[i]));
				set.insert(toUpperCase(chars[i]));
			}
		}

		charMap = static_cast<char*>(malloc(set.size() + 1));
		size_t i = 0;
		for(char c : set){
			charMap[i++] = c;
		}
		charMap[i] = 0;
	}

	lv_textarea_set_accepted_chars(entry, charMap);

	capsText = lv_label_create(obj);
	lv_obj_set_width(capsText, 14);
	lv_obj_set_style_pad_bottom(capsText, 2, 0);
	lv_obj_set_style_text_align(capsText, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(capsText, &pixelbasic7, 0);
	lv_obj_set_style_text_color(capsText, lv_color_hex(0x8e478c), 0);
	lv_obj_add_flag(capsText, LV_OBJ_FLAG_HIDDEN);

	inputGroup = lv_group_create();
	lv_group_add_obj(inputGroup, entry);
	lv_obj_clear_state(entry, LV_STATE_FOCUSED);

	// Start in T9: the textarea becomes a render of confirmedText + prediction,
	// so relax its length limit and seed the confirmed text with the initial value.
	T9Dict::init();
	inputMode = T9;
	confirmedText = text;
	// In T9 the textarea is a pure render of confirmedText + recolour markup.
	// Clearing accepted_chars AND max_length makes lv_textarea_set_text take its
	// direct-label path, so the "#808080 ...#" markup survives (otherwise it is
	// re-added char-by-char and the '#' is filtered out). The real length limit
	// is enforced on confirmedText in t9AppendDigit().
	lv_textarea_set_accepted_chars(entry, nullptr);
	lv_textarea_set_max_length(entry, 0);
	lv_label_set_text(capsText, "T9");
	updateTextarea();

	for(auto pair : keyMap){
		setButtonHoldTime(pair.first, 500);
	}
	setButtonHoldTime(BTN_R, 500);
	setButtonHoldAndRepeatTime(BTN_L, 250);
	setButtonHoldAndRepeatTime(BTN_LEFT, 250);
	setButtonHoldAndRepeatTime(BTN_RIGHT, 250);
}

TextEntry::~TextEntry(){
	lv_group_del(inputGroup);
	Input::getInstance()->removeListener(this);
	LoopManager::removeListener(this);
}

void TextEntry::showCaps(bool show) {
    if (!capsText) return;

    if (show) {
        lv_obj_clear_flag(capsText, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(capsText, LV_OBJ_FLAG_HIDDEN);
    }
}

void TextEntry::setText(const std::string& text){
	if(inputMode == T9){
		confirmedText = text;
		t9Digits.clear();
		t9Candidates.clear();
		t9MatchIndex = 0;
		t9PunctActive = false;
		updateTextarea();
	}else{
		lv_textarea_set_text(entry, text.c_str());
	}
}

void TextEntry::setTextColor(lv_color_t color){
	lv_obj_set_style_text_color(entry, color, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void TextEntry::setPlaceholder(const std::string& text){
	lv_textarea_set_placeholder_text(entry, text.c_str());
}

bool TextEntry::isActive() const{
	return active;
}

std::string TextEntry::getText() const{
	if(inputMode == T9){
		// The textarea may hold recolour markup; return the clean text instead.
		std::string t = confirmedText;
		if(!t9Digits.empty()) t += currentT9Word();
		return t;
	}
	return lv_textarea_get_text(entry);
}

void TextEntry::clear(){
    setText("");
}

void TextEntry::setCannedMessage(uint8_t button, const std::string& text){

    if(!keyMap.count(button)){
        printf("setCannedMessage rejected button=%u\n", (unsigned) button);
        return;
    }

    if(text.empty()){
        cannedMessages.erase(button);
        printf("setCannedMessage erased button=%u\n", (unsigned) button);
        return;
    }

    cannedMessages[button] = text;
}

void TextEntry::clearCannedMessage(uint8_t button){
    cannedMessages.erase(button);
}

void TextEntry::loadCannedMessages(){
    // Slot order matches CannedService: 0..8 -> keys 1..9, 9 -> key 0. Empty
    // slots are simply not registered, leaving that button's long-press inert.
    static const uint8_t CannedButtons[CannedService::Count] = {
        BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7, BTN_8, BTN_9, BTN_0
    };

    cannedMessages.clear();
    for(size_t i = 0; i < CannedService::Count; i++){
        const std::string& text = Canned.get(i);
        if(!text.empty()) cannedMessages[CannedButtons[i]] = text;
    }
}

void TextEntry::start(){
	Input::getInstance()->addListener(this);

	btnRHeld = false;
	quickReturnUsed = false;   // each editing session gets the T9->aa->T9 detour once

	lv_obj_add_state(obj, LV_STATE_EDITED);
	active = true;

	activeGroup = InputLVGL::getInstance()->getIndev()->group;
	lv_indev_set_group(InputLVGL::getInstance()->getIndev(), inputGroup);
	focus();

	lv_obj_add_event_cb(entry, [](lv_event_t* e){
		auto* entry = static_cast<TextEntry*>(e->user_data);
		if(!entry->active) return;

		entry->stop();
		lv_event_send(entry->obj, EV_ENTRY_DONE, nullptr);
	}, LV_EVENT_PRESSED, this);

	lv_obj_add_event_cb(entry, [](lv_event_t* e){
		auto* entry = static_cast<TextEntry*>(e->user_data);
		if(!entry->active) return;

		entry->stop();
		lv_event_send(entry->obj, EV_ENTRY_CANCEL, nullptr);
	}, LV_EVENT_CANCEL, this);
}

void TextEntry::stop(){
	lv_obj_clear_state(obj, LV_STATE_EDITED);
	lv_obj_remove_event_cb_with_user_data(entry, nullptr, this);

	if(activeGroup != nullptr){
		lv_indev_set_group(InputLVGL::getInstance()->getIndev(), activeGroup);
		activeGroup = nullptr;
	}
	defocus();

	Input::getInstance()->removeListener(this);
	active = false;

	if(inputMode == T9){
		// Fold any in-progress word into the textarea as plain text so getText()
		// and the rendered text are both clean once editing ends.
		finishPunct();
		t9CommitWord(false);
		lv_textarea_set_text(entry, confirmedText.c_str());
	}else if(currentKey != -1 && inputMode == MULTI_SINGLE){
		setInputMode(MULTI_LOWER);
	}
	btnRHeld = false;

	LoopManager::removeListener(this);
	currentKey = -1;
	keyTime = 0;
}

void TextEntry::focus(){
	lv_group_focus_obj(entry);
}

void TextEntry::defocus(){
	lv_obj_clear_state(entry, LV_STATE_FOCUSED);
}

void TextEntry::backspace(){
	lv_textarea_del_char(entry);
}

// ── Multi-tap key entry (non-T9 modes) ──────────────────────────────────────

void TextEntry::keyPress(uint8_t i){
	if(inputMode == T9){
		t9ButtonPressed(i);
		return;
	}

	if(i == BTN_L){
		backspace();
		return;
	}

	if(!keyMap.count(i)) return;
	uint8_t key = keyMap.at(i);
	const char* chars = characters[key];

	if(inputMode == MULTI_NUMERIC){
		// Type the numeric glyph (last char of the set) directly, no cycling.
		if(getText().size() == lv_textarea_get_max_length(entry)) return;
		lv_textarea_add_char(entry, chars[strnlen(chars, 16) - 1]);
		currentKey = -1;
		keyTime = 0;
		return;
	}

	if(key == currentKey && keyTime != 0){
		index = (index + 1) % strnlen(chars, 16);
		char character = chars[index];
		if(inputMode == MULTI_SINGLE || inputMode == MULTI_UPPER){
			character = toUpperCase(character);
		}

		lv_textarea_del_char(entry);
		lv_textarea_add_char(entry, character);

		if(lv_obj_get_scroll_x(entry) > 0){
			lv_obj_scroll_to_x(entry, LV_COORD_MAX, LV_ANIM_OFF);
		}
	}else{
		if(getText().size() == lv_textarea_get_max_length(entry)) return;

		if(currentKey != -1 && currentKey != key && inputMode == MULTI_SINGLE){
			setInputMode(MULTI_LOWER);
		}

		currentKey = key;
		index = 0;
		char character = chars[index];
		if(inputMode == MULTI_SINGLE || inputMode == MULTI_UPPER){
			character = toUpperCase(character);
		}

		lv_textarea_add_char(entry, character);
	}

	if(keyTime == 0){
		LoopManager::addListener(this);
	}

	keyTime = millis();
}

// ── T9 predictive entry ─────────────────────────────────────────────────────

std::vector<std::string> TextEntry::buildCandidates(const std::string& digits) const{
	std::vector<std::string> out;
	std::unordered_set<std::string> seen;

	// --- DEBUG: narrowing down the post-word-completion crash/reboot ---------
	uint32_t heapBefore = ESP.getFreeHeap();
	printf("[T9 DEBUG] buildCandidates('%s') heapBefore=%u\n", digits.c_str(), (unsigned) heapBefore);

	auto custom = CustomDict.getMatches(digits, 8);
	printf("[T9 DEBUG]   CustomDict.getMatches -> %u results, heap=%u\n",
		   (unsigned) custom.size(), (unsigned) ESP.getFreeHeap());

	auto dict = T9Dict::getMatches(digits, 16);
	printf("[T9 DEBUG]   T9Dict.getMatches -> %u results, heap=%u\n",
		   (unsigned) dict.size(), (unsigned) ESP.getFreeHeap());

	// Custom (learned) words first, then the static dictionary.
	for(const auto& p : custom){
		if(seen.insert(p.first).second) out.push_back(p.first);
	}
	for(const auto& p : dict){
		if(seen.insert(p.first).second) out.push_back(p.first);
	}

	// Present shortest candidates first: a word matching the typed digit count
	// exactly leads, then progressively longer completions reached via up/down.
	// stable_sort keeps the within-length ordering (custom-before-dict, each by
	// frequency) established above.
	std::stable_sort(out.begin(), out.end(),
					 [](const std::string& a, const std::string& b){
						 return a.size() < b.size();
					 });

	printf("[T9 DEBUG]   buildCandidates done: %u merged, heapAfter=%u (delta=%d)\n",
		   (unsigned) out.size(), (unsigned) ESP.getFreeHeap(),
		   (int) heapBefore - (int) ESP.getFreeHeap());
	return out;
}

void TextEntry::t9AppendDigit(char digit){
	if(maxLength != (uint32_t) -1 && confirmedText.size() >= maxLength){
		Buzz.emitBeep();
		return;
	}

	std::string trial = t9Digits + digit;
	printf("[T9 DEBUG] t9AppendDigit('%c') trial='%s' confirmedText='%s' heap=%u\n",
		   digit, trial.c_str(), confirmedText.c_str(), (unsigned) ESP.getFreeHeap());

	std::vector<std::string> cands = buildCandidates(trial);
	if(cands.empty()){
		// No dictionary or custom word starts with this digit sequence.
		Buzz.emitBeep();
		return;
	}

	t9Digits = trial;
	t9Candidates = std::move(cands);
	t9MatchIndex = 0;
	updateTextarea();
	printf("[T9 DEBUG] t9AppendDigit done, heap=%u\n", (unsigned) ESP.getFreeHeap());
}

void TextEntry::t9Backspace(){
	finishPunct();

	if(!t9Digits.empty()){
		t9Digits.pop_back();
		if(t9Digits.empty()){
			t9Candidates.clear();
		}else{
			t9Candidates = buildCandidates(t9Digits);
		}
		t9MatchIndex = 0;
		updateTextarea();
		return;
	}

	// No in-progress word: delete the last confirmed character.
	if(!confirmedText.empty()){
		confirmedText.pop_back();
		updateTextarea();
	}
}

void TextEntry::t9CommitWord(bool appendSpace){
	printf("[T9 DEBUG] t9CommitWord(appendSpace=%d) enter: t9Digits='%s' word='%s' confirmedText='%s' heap=%u\n",
		   (int) appendSpace, t9Digits.c_str(), currentT9Word().c_str(),
		   confirmedText.c_str(), (unsigned) ESP.getFreeHeap());

	if(!t9Digits.empty()){
		confirmedText += currentT9Word();
	}
	if(appendSpace){
		confirmedText += ' ';
	}
	t9Digits.clear();
	t9Candidates.clear();
	t9MatchIndex = 0;

	printf("[T9 DEBUG] t9CommitWord -> confirmedText='%s' (len=%u) heap=%u, calling updateTextarea\n",
		   confirmedText.c_str(), (unsigned) confirmedText.size(), (unsigned) ESP.getFreeHeap());

	updateTextarea();

	printf("[T9 DEBUG] t9CommitWord done, heap=%u\n", (unsigned) ESP.getFreeHeap());
}

void TextEntry::t9CyclePunct(){
	// Commit any in-progress word first (no trailing space).
	if(!t9Digits.empty()) t9CommitWord(false);

	size_t len = strlen(T9_PUNCT);
	if(t9PunctActive && (millis() - keyTime) < 1000){
		if(!confirmedText.empty()) confirmedText.pop_back();
		t9PunctIndex = (t9PunctIndex + 1) % len;
	}else{
		t9PunctActive = true;
		t9PunctIndex = 0;
		LoopManager::addListener(this);
	}
	confirmedText += T9_PUNCT[t9PunctIndex];
	keyTime = millis();
	updateTextarea();
}

void TextEntry::finishPunct(){
	if(t9PunctActive){
		t9PunctActive = false;
		keyTime = 0;
		LoopManager::removeListener(this);
	}
}

std::string TextEntry::currentT9Word() const{
	if(t9Candidates.empty()) return std::string();
	std::string w = t9Candidates[t9MatchIndex];
	if(sentenceStart() && !w.empty()){
		w[0] = toUpperCase(w[0]);
	}
	return w;
}

void TextEntry::learnLastWord(const std::string& text){
	// Isolate the final whitespace-delimited token and hand it to CustomDict,
	// which ignores anything under 2 letters or non-alphabetic. Learning just the
	// last word (rather than the whole line) targets the word the user just
	// finished in multi-tap and avoids re-counting earlier words on every toggle;
	// the send path still learns the full message as a backstop.
	size_t end = text.find_last_not_of(' ');
	if(end == std::string::npos) return;
	size_t start = text.find_last_of(' ', end);
	start = (start == std::string::npos) ? 0 : start + 1;
	CustomDict.learnText(text.substr(start, end - start + 1));
}

bool TextEntry::sentenceStart() const{
	int i = (int) confirmedText.size() - 1;
	while(i >= 0 && confirmedText[i] == ' ') i--;
	if(i < 0) return true;
	char c = confirmedText[i];
	return c == '.' || c == '!' || c == '?';
}

void TextEntry::updateTextarea(){
	std::string text = confirmedText;
	if(!t9Digits.empty()){
		text += "#808080 ";
		text += currentT9Word();
		text += "#";
	}

	printf("[T9 DEBUG] updateTextarea: text='%s' (len=%u) heap=%u, before lv_textarea_set_text\n",
		   text.c_str(), (unsigned) text.size(), (unsigned) ESP.getFreeHeap());

	lv_textarea_set_text(entry, text.c_str());

	printf("[T9 DEBUG] updateTextarea: lv_textarea_set_text returned, heap=%u, scrolling\n",
		   (unsigned) ESP.getFreeHeap());

	lv_obj_scroll_to_x(entry, LV_COORD_MAX, LV_ANIM_OFF);

	printf("[T9 DEBUG] updateTextarea done, heap=%u\n", (unsigned) ESP.getFreeHeap());
}

void TextEntry::t9ButtonPressed(uint i){
	if(i == BTN_ENTER || i == BTN_BACK) return;   // done / cancel handled by LVGL
	if(i == BTN_R) return;                         // mode cycle happens on release

	if(i == BTN_L){
		t9Backspace();
		return;
	}

	// BTN_LEFT / BTN_RIGHT (== BTN_UP / BTN_DOWN aliases) cycle candidates.
	if(i == BTN_LEFT || i == BTN_RIGHT){
		if(t9Candidates.empty()) return;
		finishPunct();
		int n = (int) t9Candidates.size();
		if(i == BTN_RIGHT) t9MatchIndex = (t9MatchIndex + 1) % n;
		else               t9MatchIndex = (t9MatchIndex + n - 1) % n;
		updateTextarea();
		return;
	}

	if(!keyMap.count(i)) return;
	uint8_t key = keyMap.at(i);

	if(key == 0){             // BTN_1 -> punctuation
		t9CyclePunct();
		return;
	}
	if(key == 9){             // BTN_0 -> confirm word + space
		finishPunct();
		t9CommitWord(true);
		return;
	}

	// keys 1..8 map to T9 digits '2'..'9'.
	finishPunct();
	t9AppendDigit((char) ('1' + key));
}

// ── Button routing ──────────────────────────────────────────────────────────

void TextEntry::buttonPressed(uint i){
	if(inputMode == T9){
		t9ButtonPressed(i);
		return;
	}

	if(i == BTN_LEFT || i == BTN_RIGHT){
		if(currentKey != -1 && inputMode == MULTI_SINGLE){
			setInputMode(MULTI_LOWER);
		}

		keyTime = 0;
		currentKey = -1;
		LoopManager::removeListener(this);
		lv_obj_set_style_anim_time(entry, 500, LV_PART_CURSOR | LV_STATE_FOCUSED);

		if(i == BTN_RIGHT){
			lv_textarea_cursor_right(entry);
		}else if(i == BTN_LEFT){
			lv_textarea_cursor_left(entry);
		}
		return;
	}

	if(i == BTN_ENTER || i == BTN_BACK) return;

	if(i == BTN_R) return;

	keyPress(i);
}

void TextEntry::buttonHeldRepeat(uint i, uint repeatCount){
	if(inputMode == T9){
		if(i == BTN_L) t9Backspace();
		return;
	}

	if(i == BTN_L){
		backspace();
	}else if(i == BTN_RIGHT){
		lv_textarea_cursor_right(entry);
	}else if(i == BTN_LEFT){
		lv_textarea_cursor_left(entry);
	}
}

void TextEntry::buttonHeld(uint i){
    if(i == BTN_R){
        btnRHeld = true;
        return;
    }

	auto canned = cannedMessages.find(i);
    if(canned != cannedMessages.end()){
		if(inputMode == T9){
			finishPunct();
			if(!t9Digits.empty()) t9CommitWord(false);
			if(!confirmedText.empty() && confirmedText.back() != ' '){
				confirmedText += ' ';
			}
			confirmedText += canned->second;
			updateTextarea();
			return;
		}

        std::string text = getText();
        if(currentKey != -1){
            lv_textarea_del_char(entry);
            text = getText();
        }

        if(!text.empty() && text.back() != ' '){
            text += " ";
        }

        text += canned->second;
        setText(text);

        keyTime = 0;
        currentKey = -1;
        LoopManager::removeListener(this);
        return;
    }

	if(inputMode == T9) return;   // no hold-to-last-char in T9

    if(!keyMap.count(i)){
        return;
    }

    uint8_t key = keyMap.at(i);
    if(key != currentKey){
        return;
    }

    const char* chars = characters[key];
    char last = chars[strnlen(chars, 16) - 1];

    lv_textarea_del_char(entry);
    lv_textarea_add_char(entry, last);

    keyTime = 0;
    currentKey = -1;
    LoopManager::removeListener(this);
}

void TextEntry::buttonReleased(uint i){
	if((i == BTN_LEFT || i == BTN_RIGHT) && inputMode != T9){
		lv_async_call([](void* user_data){
			auto obj = static_cast<lv_obj_t*>(user_data);
			lv_event_send(obj, EV_ENTRY_LR, nullptr);
		}, obj);
	}

	if(i != BTN_R) return;

	if(btnRHeld){
		btnRHeld = false;
		return;
	}

	if(inputMode != T9 && keyTime != 0){
		LoopManager::removeListener(this);
		keyTime = 0;
		currentKey = -1;
	}

	if(inputMode == T9){
		if(!quickReturnUsed){
			// First BTN_R from T9 -> aa: a quick detour for a one-off word edit.
			setInputMode(MULTI_LOWER);
		}else{
			// Detour already spent: enter the deliberate caps cycle at Aa,
			// skipping the "aa" quick-edit mode.
			setInputMode(MULTI_SINGLE);
		}
	}else if(inputMode == MULTI_LOWER && !quickReturnUsed){
		// The one-time press back out of "aa" returns straight to T9 instead of
		// continuing through Aa/AA/12. Latch it so it never happens again.
		setInputMode(T9);
		quickReturnUsed = true;
	}else{
		// Normal mode cycle, wrapping back to T9.
		setInputMode((InputMode) ((inputMode + 1) % InputMode::COUNT));
	}
}

void TextEntry::loop(uint micros){
	if(millis() - keyTime < 1000) return;

	if(inputMode == T9){
		finishPunct();
		return;
	}

	keyTime = 0;
	currentKey = -1;
	lv_obj_set_style_anim_time(entry, 500, LV_PART_CURSOR | LV_STATE_FOCUSED);
	LoopManager::removeListener(this);

	if(inputMode == MULTI_SINGLE){
		setInputMode(MULTI_LOWER);
	}
}

void TextEntry::setInputMode(TextEntry::InputMode mode){
	InputMode old = inputMode;

	if(old == T9 && mode != T9){
		// Leaving T9: bake the prediction into the textarea as plain editable text.
		finishPunct();
		t9CommitWord(false);
		lv_textarea_set_accepted_chars(entry, charMap);
		lv_textarea_set_max_length(entry, maxLength);
		lv_textarea_set_text(entry, confirmedText.c_str());
		lv_textarea_set_cursor_pos(entry, LV_TEXTAREA_CURSOR_LAST);
	}

	inputMode = mode;

	if(mode == T9 && old != T9){
		// Entering T9: adopt the current textarea text as the confirmed base.
		confirmedText = lv_textarea_get_text(entry);
		// Learn the word just typed in multi-tap (e.g. switching to "aa" for a
		// word T9 didn't know) so it is persisted AND becomes an immediate T9
		// candidate -- before the message is ever sent.
		learnLastWord(confirmedText);
		t9Digits.clear();
		t9Candidates.clear();
		t9MatchIndex = 0;
		t9PunctActive = false;
		lv_textarea_set_accepted_chars(entry, nullptr);
		lv_textarea_set_max_length(entry, 0);
		updateTextarea();
	}

	const char* names[] = { "T9", "aa", "Aa", "AA", "12" };
	lv_label_set_text(capsText, names[mode]);
}
