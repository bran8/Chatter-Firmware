#include "GamesScreen.h"
#include <Pins.hpp>
#include <Chatter.h>
#include <LITTLEFS.h>
#include "../Model/Profile.hpp"
#include "../Fonts/font.h"
#include "../Types.hpp"
#include "../Storage/Storage.h"
#include "../Elements/User.h"
#include "../Elements/ListItem.h"
#include "PairScreen.h"
#include "ProfileScreen.h"
#include "../FSLVGL.h"
#include "../Elements/GameItem.h"

// Headless-wifi branch: there's no LCD to play games on, so the actual game
// implementations (Space/Invaders/Snake) and their data/Games assets were
// removed from this branch entirely. This screen is unreachable anyway (no
// MainMenu is ever created in the headless boot path) but stays as an empty
// shell so GameEngine/Game.h (still needed by SleepService/ShutdownService)
// keeps compiling.
const GamesScreen::GameInfo GamesScreen::Games[] = {};

GamesScreen::GamesScreen() : LVScreen(), apop(this){
	lv_obj_set_height(obj, LV_SIZE_CONTENT);
	lv_obj_set_layout(obj,LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_ACTIVE);
	lv_obj_set_style_pad_gap(obj, 0, 0);
	lv_obj_set_style_pad_all(obj, 3, 0);

	new ListItem(obj, "Games");

	for(const auto& game : Games){
		auto listItem = new GameItem(obj, game.name, game.icon);

		lv_group_add_obj(inputGroup, listItem->getLvObj());
		// lv_obj_add_flag(listItem->getLvObj(), LV_OBJ_FLAG_SCROLL_ON_FOCUS);

		lv_obj_add_event_cb(listItem->getLvObj(), [](lv_event_t* e){
			auto screen = static_cast<GamesScreen*>(e->user_data);
			screen->stop();

			auto gameId = lv_obj_get_index(e->target) - 1;

			LoopManager::defer([gameId, screen](uint32_t dt){
				FSLVGL::unloadCache();

				auto info = Games[gameId];

				uint32_t splashStart = 0;
				if(info.splash){
					auto display = Chatter.getDisplay();
					display->getBaseSprite()->drawIcon(LITTLEFS.open(info.splash), 0, 0, 160, 128);
					display->commit();
					splashStart = millis();
				}

				auto game = info.launch(screen);

				game->load();
				while(!game->isLoaded()){
					delay(1);
				}

				while(millis() - splashStart < 2000){
					delay(10);
				}

				game->start();
			});
		}, LV_EVENT_PRESSED, this);
	}
}

void GamesScreen::onStart(){
	apop.start();
}

void GamesScreen::onStop(){
	apop.stop();
}