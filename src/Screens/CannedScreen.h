#ifndef CHATTER_FIRMWARE_CANNEDSCREEN_H
#define CHATTER_FIRMWARE_CANNEDSCREEN_H

#include "../Interface/LVScreen.h"
#include "../Services/CannedService.h"

/**
 * Lists the 10 canned-message slots (key label + current text) plus a
 * "Reset to defaults" action. Selecting a slot opens its editor. The list is
 * refreshed in onStarting(), which re-fires when an editor pops back, so edits
 * show immediately.
 */
class CannedScreen : public LVScreen {
public:
	CannedScreen();
	virtual ~CannedScreen();

	void onStarting() override;

private:
	lv_style_t style_def;
	lv_style_t style_focused;

	lv_obj_t* rowLabels[CannedService::Count];
	lv_obj_t* resetRow;

	void refresh();
};

#endif //CHATTER_FIRMWARE_CANNEDSCREEN_H
