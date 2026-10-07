#include "Engine/Core/App.h"
#include "Engine/Vendor/SDL2/include/SDL2/SDL_stdinc.h"
#include "Engine/Vendor/SDL2/include/SDL2/SDL_timer.h"
#include <algorithm>
#include <cmath>

int main(int argv, char** args) {

    App app;
    app.init("Config/settings.json");
    if (!app.isRunning) return 1;

    if (app.audio.loadMusic("menu", "Assets/Audio/Music/runic_sanctuary_hopeful_loop.ogg")) {
        app.audio.playMusic("menu", true);
    }
    app.audio.loadSound(
            "button.click",
            "Assets/Audio/Sounds/rune_button_click.wav"
    );

    const double timerFrequency = static_cast<double>(SDL_GetPerformanceFrequency());

    while(app.isRunning) {

        const Uint64 frameStart = SDL_GetPerformanceCounter();

        app.handleEvents();
        app.update();
        app.render();

        const double frameTime = (SDL_GetPerformanceCounter() - frameStart) * 1000.0 / timerFrequency;
        const double frameDelay = 1000.0 / std::max(1, app.settings.fpsLimit);
        if(frameDelay > frameTime){
            SDL_Delay(static_cast<Uint32>(std::ceil(frameDelay - frameTime)));
        }
    }

    return 0;
}
