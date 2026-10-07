#define SDL_MAIN_HANDLED
#include "../Engine/Scenes/UIScene.h"
#include "../Engine/Scenes/SceneManager.h"
#include "../Engine/Vendor/SDL2/include/SDL2/SDL_image.h"
#include "../Engine/Vendor/SDL2/include/SDL2/SDL_mixer.h"
#include "../Engine/Vendor/SDL2/include/SDL2/SDL_test_memory.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>

// Track C++ ownership separately from SDL's own allocation counter.
static std::atomic<long long> cppAllocations{0};

enum class Failure { None, SDL, Window, Renderer, Font, Image, Mixer, Audio };
static Failure failure = Failure::None;

// Linker wrappers inject failures while all successful calls use the real SDL.
extern "C" {
int SDLCALL __real_SDL_Init(Uint32 flags);
SDL_Window* SDLCALL __real_SDL_CreateWindow(const char*, int, int, int, int, Uint32);
SDL_Renderer* SDLCALL __real_SDL_CreateRenderer(SDL_Window*, int, Uint32);
int SDLCALL __real_TTF_Init();
int SDLCALL __real_IMG_Init(int flags);
int SDLCALL __real_Mix_Init(int flags);
int SDLCALL __real_Mix_OpenAudio(int, Uint16, int, int);

int SDLCALL __wrap_SDL_Init(Uint32 flags) {
    if(failure == Failure::SDL) {
        // Exercise cleanup when SDL has initialized only part of the request.
        __real_SDL_Init(SDL_INIT_VIDEO);
        return SDL_SetError("injected SDL initialization failure");
    }
    return __real_SDL_Init(flags);
}
SDL_Window* SDLCALL __wrap_SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags) {
    if(failure == Failure::Window) {
        SDL_SetError("injected window creation failure");
        return nullptr;
    }
    return __real_SDL_CreateWindow(title, x, y, w, h, flags);
}
SDL_Renderer* SDLCALL __wrap_SDL_CreateRenderer(SDL_Window* window, int index, Uint32 flags) {
    if(failure == Failure::Renderer) {
        SDL_SetError("injected renderer creation failure");
        return nullptr;
    }
    return __real_SDL_CreateRenderer(window, index, flags);
}
int SDLCALL __wrap_TTF_Init() {
    if(failure == Failure::Font) return SDL_SetError("injected font initialization failure");
    return __real_TTF_Init();
}
int SDLCALL __wrap_IMG_Init(int flags) {
    if(flags && failure == Failure::Image) {
        __real_IMG_Init(flags);
        SDL_SetError("injected image codec initialization failure");
        return 0;
    }
    return __real_IMG_Init(flags);
}
int SDLCALL __wrap_Mix_Init(int flags) {
    if(flags && failure == Failure::Mixer) {
        __real_Mix_Init(flags);
        SDL_SetError("injected audio codec initialization failure");
        return 0;
    }
    return __real_Mix_Init(flags);
}
int SDLCALL __wrap_Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize) {
    if(failure == Failure::Audio) return SDL_SetError("injected audio device initialization failure");
    return __real_Mix_OpenAudio(frequency, format, channels, chunksize);
}
}

void* operator new(std::size_t size) {
    if(void* pointer = std::malloc(size ? size : 1)) {
        ++cppAllocations;
        return pointer;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept {
    if(pointer) {
        --cppAllocations;
        std::free(pointer);
    }
}
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

static void require(bool condition, const char* message) {
    if(!condition) throw std::runtime_error(message);
}

struct Allocations {
    int sdl = SDL_GetNumAllocations();
    long long cpp = cppAllocations.load();

    void check(const char* phase) const {
        const int remainingSDL = SDL_GetNumAllocations();
        const long long remainingCpp = cppAllocations.load();
        if(sdl != remainingSDL || cpp != remainingCpp) {
            std::fprintf(stderr, "%s: SDL %d -> %d, C++ %lld -> %lld\n",
                         phase, sdl, remainingSDL, cpp, remainingCpp);
            throw std::runtime_error("Outstanding allocations changed");
        }
    }
};

static void requireClosed(const App& app) {
    require(!app.isRunning && !app.window && !App::renderer && !App::sceneManager,
            "App still owns resources after cleanup");
    require(SDL_WasInit(0) == 0 && TTF_WasInit() == 0 && IMG_Init(0) == 0 &&
            Mix_Init(0) == 0 && Mix_QuerySpec(nullptr, nullptr, nullptr) == 0,
            "SDL, fonts, image codecs or audio were left initialized");
}

static void emptyScene(App& app) {
    App::sceneManager->closeScene(app);
    app.render();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
}

static void exerciseScenes(App& app) {
    for(auto factory : {createScene1, createSettingsScene, createScene2}) {
        App::sceneManager->changeScene(factory, app);
        app.update();
        app.render();
    }
    emptyScene(app);
}

// Uses the base UIScene destructor, without a custom onExit implementation.
static void exerciseUIOwnership(App& app) {
    SceneManager manager;
    manager.changeScene([]() { return new UIScene(); }, app);
    auto* scene = static_cast<UIScene*>(manager.activeScene);
    scene->uiLayer = new UILayer(app);
    UIBox* box = scene->uiLayer->addBox({0,0,600,400}, {0,0,0,0}, 24,
                                       "test", false, "assets/UI/WindowBG2.png");
    Font font("assets/fonts/Lato-Regular.ttf", 24);
    require(font.font != nullptr, TTF_GetError());
    Label* label = box->addLabel(font, "first", {0,0,0,0}, {10,10,200,48}, "assets/UI/btn.png");
    Input* input = box->addInput(font, "placeholder", {10,70,200,48});
    Select* select = box->addSelect(font, {"one", "two", "three"}, 0, {10,130,200,48});
    box->addButton(font, "button", {0,0,0,0}, {10,190,200,48}, 1.05f, {}, "assets/UI/btn.png");
    for(int i = 0; i < 50; ++i) {
        label->updateText("text " + std::to_string(i));
        input->setText("input " + std::to_string(i));
        select->setSelectedIndex(i % 3);
        require(label->setBackgroundImage("assets/UI/btn.png"), "PNG reload failed");
        require(font.changeSize(16 + i % 16), "Font reload failed");
    }
    // A scene manager going out of scope must also destroy its active UI scene.
}

int main() {
    try {
        require(SDLTest_TrackAllocations() == 0, "Cannot enable SDL allocation tracking");
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
        SDL_setenv("SDL_RENDER_DRIVER", "", 1);
        std::cout << "Memory regression check\n";
        // Warm up runtime/library caches before recording the shutdown baseline.
        {
            App warmup;
            warmup.init("Config/settings.json");
            require(warmup.isRunning, "Warmup initialization failed");
            SDL_RendererInfo info = {};
            require(SDL_GetRendererInfo(App::renderer, &info) == 0 &&
                    (info.flags & SDL_RENDERER_SOFTWARE),
                    "Headless initialization must choose a software renderer");
            exerciseScenes(warmup);
            exerciseUIOwnership(warmup);
            warmup.clean();
            requireClosed(warmup);
        }
        const Allocations shutdownBaseline;
        {
            App app;
            app.init("Config/settings.json");
            require(app.isRunning, "App initialization failed");
            exerciseScenes(app);
            exerciseUIOwnership(app);
            emptyScene(app);
            const Allocations sceneBaseline;
            for(int i = 0; i < 100; ++i) {
                exerciseScenes(app);
                sceneBaseline.check("Scene transitions");
            }
            exerciseUIOwnership(app);
            sceneBaseline.check("UI, texture and font ownership");
            app.clean();
            requireClosed(app);
            app.clean();
            requireClosed(app);
            app.update();
            app.render();
        }
        shutdownBaseline.check("Explicit cleanup and destructor");
        {
            App app;
            app.init("Config/settings.json");
            require(app.isRunning, "Reinitialization failed");
            app.init("Config/settings.json");
            require(app.isRunning, "Repeated initialization failed");
            // No explicit clean: App's destructor must release everything.
        }
        shutdownBaseline.check("App destructor and repeated initialization");
        {
            App app;
            app.init("Config/does-not-exist.json");
            requireClosed(app);
            SDL_setenv("SDL_AUDIODRIVER", "does-not-exist", 1);
            app.init("Config/settings.json");
            requireClosed(app);
            SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
        }
        shutdownBaseline.check("Failed initialization");
        for(Failure injected : {Failure::SDL, Failure::Window, Failure::Renderer,
                                Failure::Font, Failure::Image, Failure::Mixer, Failure::Audio}) {
            {
                App app;
                failure = injected;
                app.init("Config/settings.json");
                failure = Failure::None;
                requireClosed(app);
                app.clean();
            }
            shutdownBaseline.check("Injected initialization failure");
        }
        std::printf("PASS: 300 scene transitions, UI/text/PNG/font updates, repeated init, "
                    "cleanup, destructors and 7 startup failure paths; "
                    "SDL=%d, C++=%lld (unchanged baseline).\n",
                    shutdownBaseline.sdl, shutdownBaseline.cpp);
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        SDLTest_LogAllocations();
        return 1;
    }
}
