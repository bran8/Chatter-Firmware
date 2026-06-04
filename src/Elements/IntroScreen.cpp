#include "IntroScreen.h"
#include "../Screens/MainMenu.h"

IntroScreen::IntroScreen(void (* callback)())
    : callback(callback), gif(nullptr) {
    // Intentionally skip creating the intro GIF.
}

void IntroScreen::onStart() {
    static bool forwarded = false;
    if (forwarded) return;
    forwarded = true;

    MainMenu* menu = new MainMenu();
    menu->start();

    if (callback != nullptr) {
        callback();
    }
}

void IntroScreen::onStop() {
    gif = nullptr;
}