#include "Engine/Core/App.h"
#include "Engine/Vendor/SDL2/include/SDL2/SDL_stdinc.h"
#include "Engine/Vendor/SDL2/include/SDL2/SDL_timer.h"

App *app = nullptr;

int main(int argv, char** args) {

    app = new App;
    app->init("Config/settings.json");

    constexpr int FPS = 60;
    constexpr int frameDelay = 1000 / FPS;

    while(app->isRunning) {

        const Uint32 frameStart = SDL_GetTicks();

        app->handleEvents();
        app->update();
        app->render();

        if(const int frameTime = SDL_GetTicks() - frameStart; frameDelay > frameTime){
            SDL_Delay(frameDelay - frameTime);
        }
    }

    app->clean();

    return 0;
}