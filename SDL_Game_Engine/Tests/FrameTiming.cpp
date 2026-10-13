#define SDL_MAIN_HANDLED
#define main gameMain
#include "../main.cpp"
#undef main
#include "../Engine/Scenes/SceneManager.h"
#include <cstdio>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if(!condition) throw std::runtime_error(message);
}

extern "C" SDL_Renderer* SDLCALL __real_SDL_CreateRenderer(SDL_Window*, int, Uint32);
extern "C" SDL_Renderer* SDLCALL __wrap_SDL_CreateRenderer(SDL_Window* window, int index, Uint32 flags) {
    require((flags & SDL_RENDERER_PRESENTVSYNC) == 0,
            "Manual FPS limiting must not also request VSync");
    return __real_SDL_CreateRenderer(window, index, flags);
}

static bool finished = false;
static bool quitReceived = false;

// Exercise the actual main loop with a scene that measures rendering and updates.
class FrameTimingScene : public Scene {
    static constexpr int limitCount = 9;
    const int limits[limitCount] = {1, 5, 10, 30, 60, 120, 144, 240, 5};
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    int phase = 0;
    int frames = 0;
    int updates = 0;
    int updatesSinceRender = 0;
    int stallStage = 0;
    Uint64 phaseStart = 0;

public:
    void onEnter(App& app) override { app.settings.fpsLimit = limits[0]; }

    void handleEvents(App&, SDL_Event& event) override {
        if(event.type == SDL_QUIT) quitReceived = true;
    }

    void update(App&) override {
        require(std::abs(App::deltaTime - 1.0f / 60.0f) < 0.000001f,
                "Updates must receive a fixed deltaTime of 1/60 second");
        ++updates;
        ++updatesSinceRender;
    }

    void render(App& app) override {
        require(!finished, "Main loop rendered again after SDL_QUIT");
        if(app.settings.fpsLimit == 0 || app.settings.fpsLimit >= 30)
            require(updatesSinceRender <= 5, "A stalled frame caused too many catch-up updates");
        const int frameUpdates = updatesSinceRender;
        updatesSinceRender = 0;
        SDL_SetRenderDrawColor(App::renderer, 0, 0, 0, 255);
        SDL_RenderClear(App::renderer);

        const Uint64 now = SDL_GetPerformanceCounter();
        if(!phaseStart) {
            phaseStart = now;
            frames = 1;
            updates = 0;
            return;
        }
        const double elapsed = (now - phaseStart) / frequency;

        if(phase < limitCount) {
            ++frames;
            if(elapsed < 2.1) return;
            const double fps = (frames - 1) / elapsed;
            const double updateRate = updates / elapsed;
            std::printf("Limit %d: %.2f FPS, %.2f updates/s, currentFPS=%d\n",
                        limits[phase], fps, updateRate, app.currentFPS);
            require(fps >= limits[phase] * 0.90 && fps <= limits[phase] * 1.02,
                    "Measured FPS does not match the selected limit");
            require(updateRate >= 55.0 && updateRate <= 65.0,
                    "Logic update rate changed with the rendering FPS limit");
            require(std::abs(app.currentFPS - limits[phase]) <= limits[phase] * 0.10,
                    "currentFPS does not report the measured frame rate");
            ++phase;
            phaseStart = now;
            frames = 1;
            updates = 0;
            app.settings.fpsLimit = phase < limitCount ? limits[phase] : 60;
            return;
        }

        if(stallStage == 0) {
            SDL_Delay(400);
            ++stallStage;
        } else if(stallStage == 1) {
            require(frameUpdates == 5, "Stall recovery did not use the bounded catch-up loop");
            ++stallStage;
        } else if(stallStage == 2) {
            require(frameUpdates <= 2, "Overdue updates were carried into the next frame");
            app.settings.fpsLimit = 0;
            phaseStart = now;
            frames = 1;
            updates = 0;
            ++stallStage;
        } else {
            ++frames;
            if(elapsed < 2.1) return;
            const double fps = (frames - 1) / elapsed;
            const double updateRate = updates / elapsed;
            std::printf("Unlimited: %.2f FPS, %.2f updates/s, currentFPS=%d\n",
                        fps, updateRate, app.currentFPS);
            require(fps > 240.0 && app.currentFPS > 240,
                    "Unlimited mode still applies an FPS cap");
            require(updateRate >= 55.0 && updateRate <= 65.0,
                    "Unlimited rendering changed the logic update rate");
            app.settings.fpsLimit = 60;
            SDL_Event event = {};
            event.type = SDL_QUIT;
            require(SDL_PushEvent(&event) == 1, "Cannot request a clean shutdown");
            finished = true;
        }
    }
};

Scene* createScene1() { return new FrameTimingScene(); }

int main() {
    try {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
        SDL_setenv("SDL_RENDER_DRIVER", "", 1);
        require(gameMain(0, nullptr) == 0, "Game startup failed");
        require(finished && quitReceived, "Frame timing test did not complete");
        std::printf("PASS: FPS limits, fixed updates/deltaTime, live limit changes, "
                    "stall recovery, unlimited rendering and SDL_QUIT.\n");
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
