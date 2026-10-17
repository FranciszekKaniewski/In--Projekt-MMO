#include "Engine/Core/App.h"
#include <SDL_stdinc.h>
#include <SDL_timer.h>
#include <algorithm>
#include <cmath>

int main(int argv, char** args) {

    App app;
    app.init("Config/settings.json");
    if (!app.isRunning) return 1;

    const Uint64 timerFrequency = SDL_GetPerformanceFrequency();
    const double fixedTimeStep = 1.0 / 60.0;
    double lag = 0.0;
    Uint64 lastTime = SDL_GetPerformanceCounter();
    Uint64 fpsTimer = lastTime;
    int frames = 0;

    while(app.isRunning) {

        const Uint64 frameStart = SDL_GetPerformanceCounter();
        const double elapsed = static_cast<double>(frameStart - lastTime) / timerFrequency;
        lastTime = frameStart;
        lag += elapsed;

        // Low FPS caps need more fixed steps; allow one spare for timing jitter.
        const int maxUpdatesPerFrame = std::max(5, app.settings.fpsLimit > 0
            ? static_cast<int>(std::ceil(
                1.0 / (app.settings.fpsLimit * fixedTimeStep))) + 1
            : 5);

        app.handleEvents();
        if(!app.isRunning) break;

        int updates = 0;
        while(lag >= fixedTimeStep && updates < maxUpdatesPerFrame && app.isRunning) {
            App::deltaTime = static_cast<float>(fixedTimeStep);
            app.update();
            lag -= fixedTimeStep;
            ++updates;
        }
        // Discard overdue full steps after a stall, keeping the fractional step.
        if(lag >= fixedTimeStep) lag = std::fmod(lag, fixedTimeStep);
        if(!app.isRunning) break;

        app.render();
        if(!app.isRunning) break;

        if(app.settings.fpsLimit > 0) {
            const Uint64 targetTicks = static_cast<Uint64>(std::ceil(
                static_cast<double>(timerFrequency) / app.settings.fpsLimit));
            while(true) {
                const Uint64 frameTicks = SDL_GetPerformanceCounter() - frameStart;
                if(frameTicks >= targetTicks) break;
                const double remainingMs = (targetTicks - frameTicks) * 1000.0 / timerFrequency;
                // Short sleeps avoid a large overshoot; finish the last 3 ms precisely.
                if(remainingMs > 3.0) SDL_Delay(1);
            }
        }

        ++frames;
        const Uint64 now = SDL_GetPerformanceCounter();
        const double fpsElapsed = static_cast<double>(now - fpsTimer) / timerFrequency;
        if(fpsElapsed >= 0.4) {
            app.currentFPS = static_cast<int>(std::lround(frames / fpsElapsed));
            frames = 0;
            fpsTimer = now;
        }
    }

    return 0;
}
