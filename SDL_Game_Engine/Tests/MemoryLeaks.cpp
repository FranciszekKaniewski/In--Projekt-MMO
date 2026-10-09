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

struct SceneChangeState {
    bool continued = false;
    bool exited = false;
    bool destroyed = false;
    bool replacementCreated = false;
};

class SceneChangeTest : public Scene {
    SceneChangeState& state;
    int phase;
    bool close;
    int counter = 0;

    void requestChange(App& app) {
        auto& result = state;
        if(close) {
            App::sceneManager->closeScene(app);
        } else {
            App::sceneManager->changeScene([&result]() -> Scene* {
                require(result.continued && result.exited && result.destroyed,
                        "Replacement created before the old scene finished");
                result.replacementCreated = true;
                return new UIScene();
            }, app);
        }
        require(!result.exited && !result.destroyed,
                "Scene destroyed while its method was still running");
        // Using a member after requesting a transition must remain safe.
        ++counter;
        result.continued = counter == 1;
    }

public:
    SceneChangeTest(SceneChangeState& state, int phase, bool close)
        : state(state), phase(phase), close(close) {}
    ~SceneChangeTest() override { state.destroyed = true; }
    void onExit(App&) override { state.exited = true; }
    void handleEvents(App& app, SDL_Event&) override {
        if(phase == 0) requestChange(app);
    }
    void update(App& app) override {
        if(phase == 1) requestChange(app);
    }
    void render(App& app) override {
        if(phase == 2) requestChange(app);
    }
};

static void exerciseDeferredSceneChanges(App& app) {
    for(int phase = 0; phase < 3; ++phase) {
        for(bool close : {false, true}) {
            SceneChangeState state;
            App::sceneManager->changeScene([&]() -> Scene* {
                return new SceneChangeTest(state, phase, close);
            }, app);
            if(phase == 0) {
                SDL_Event event = {};
                event.type = SDL_USEREVENT;
                App::sceneManager->handleEvents(app, event);
            } else if(phase == 1) {
                app.update();
            } else {
                app.render();
            }
            require(state.continued && state.exited && state.destroyed,
                    "Pending scene transition was not applied after the method returned");
            require(close ? App::sceneManager->activeScene == nullptr
                          : state.replacementCreated && App::sceneManager->activeScene != nullptr,
                    "Scene transition produced the wrong active scene");
            App::sceneManager->closeScene(app);
        }
    }
}

static constexpr const char* menuMusicPath =
    "Assets/Audio/Music/runic_sanctuary_hopeful_loop.ogg";

static void writeAudioFixture() {
    // One second of mono PCM silence at 22050 Hz, with a WAV header.
    static const unsigned char wav[44 + 44100] = {
        'R','I','F','F', 0x68,0xac,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
        0x22,0x56,0,0, 0x44,0xac,0,0, 2,0, 16,0,
        'd','a','t','a', 0x44,0xac,0,0
    };
    std::FILE* file = std::fopen("audio-fixture.wav", "wb");
    require(file != nullptr, "Cannot create WAV fixture");
    const bool written = std::fwrite(wav, 1, sizeof(wav), file) == sizeof(wav);
    const int closed = std::fclose(file);
    require(written && closed == 0, "Cannot write WAV fixture");
}

static void exerciseAudio(App& app) {
    require(app.audio.playSound("missing") == -1, "Unknown sound must fail safely");
    require(!app.audio.playMusic("missing"), "Unknown music must fail safely");
    require(!app.audio.loadSound("", "audio-fixture.wav"), "Empty audio ID must fail");
    require(!app.audio.loadMusic("menu", ""), "Empty audio path must fail");
    require(!app.audio.loadSound("effect", "does-not-exist.wav"), "Missing WAV must fail");
    require(!app.audio.loadMusic("menu", "does-not-exist.ogg"), "Missing OGG must fail");
    // A failed load must leave the ID available for a successful retry.
    require(app.audio.loadSound("effect", "audio-fixture.wav"), "WAV retry failed");
    require(app.audio.loadMusic("menu", menuMusicPath), "OGG retry failed");
    require(app.audio.loadSound("effect", "does-not-exist.wav"), "Sound ID was not cached");
    require(app.audio.loadMusic("menu", "does-not-exist.ogg"), "Music ID was not cached");

    require(app.audio.playMusic("menu", true), "OGG playback failed");
    require(Mix_PlayingMusic() == 1, "Music is not playing");
    app.audio.pauseMusic();
    require(Mix_PausedMusic() == 1, "Music pause failed");
    app.audio.resumeMusic();
    require(Mix_PausedMusic() == 0, "Music resume failed");
    App::sceneManager->changeScene(createSettingsScene, app);
    require(Mix_PlayingMusic() == 1, "Scene transition stopped music");

    app.settings.volume = 37;
    app.settings.muted = false;
    app.applyAudioSettings();
    const int expectedVolume = 37 * MIX_MAX_VOLUME / 100;
    const int first = app.audio.playSound("effect", -1);
    require(first >= 0, "WAV playback failed");
    require(Mix_Volume(first, -1) == expectedVolume &&
            Mix_VolumeMusic(-1) == expectedVolume,
            "Playback changed the configured volume");

    app.settings.muted = true;
    app.applyAudioSettings();
    const int second = app.audio.playSound("effect", -1);
    require(second >= 0 && second != first, "Concurrent sound playback failed");
    require(Mix_Volume(first, -1) == 0 && Mix_Volume(second, -1) == 0 &&
            Mix_VolumeMusic(-1) == 0, "Playback bypassed mute");
    app.settings.muted = false;
    app.applyAudioSettings();
    require(Mix_Volume(first, -1) == expectedVolume &&
            Mix_Volume(second, -1) == expectedVolume &&
            Mix_VolumeMusic(-1) == expectedVolume, "Unmute lost the previous volume");

    const int channelCount = Mix_AllocateChannels(-1);
    for(int i = 2; i < channelCount; ++i) {
        require(app.audio.playSound("effect", -1) >= 0, "Cannot fill mixer channels");
    }
    require(app.audio.playSound("effect") == -1, "Channel exhaustion must report failure");
    require(Mix_Playing(-1) == channelCount, "Channel exhaustion interrupted a sound");

    app.audio.stopMusic();
    require(Mix_PlayingMusic() == 0, "Music stop failed");
    require(app.audio.playMusic("menu", true), "Music restart failed");
    app.audio.clear();
    app.audio.clear();
    require(Mix_PlayingMusic() == 0 && Mix_Playing(-1) == 0, "Audio clear left playback active");
    require(!app.audio.playMusic("menu") && app.audio.playSound("effect") == -1,
            "Audio clear retained resource IDs");
    app.settings.volume = 100;
    app.applyAudioSettings();
}

static void startAudioForCleanup(App& app) {
    require(app.audio.loadMusic("cleanup.music", menuMusicPath) &&
            app.audio.playMusic("cleanup.music", true), "Cannot prepare music for cleanup");
    require(app.audio.loadSound("cleanup.sound", "audio-fixture.wav") &&
            app.audio.playSound("cleanup.sound", -1) >= 0,
            "Cannot prepare sound for cleanup");
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
        writeAudioFixture();
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
            exerciseDeferredSceneChanges(warmup);
            exerciseUIOwnership(warmup);
            exerciseAudio(warmup);
            startAudioForCleanup(warmup);
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
            exerciseAudio(app);
            emptyScene(app);
            const Allocations sceneBaseline;
            for(int i = 0; i < 100; ++i) {
                exerciseScenes(app);
                sceneBaseline.check("Scene transitions");
            }
            exerciseUIOwnership(app);
            sceneBaseline.check("UI, texture and font ownership");
            exerciseDeferredSceneChanges(app);
            sceneBaseline.check("Deferred scene changes and closure");
            startAudioForCleanup(app);
            app.clean();
            requireClosed(app);
            require(!app.audio.loadMusic("closed", menuMusicPath),
                    "Audio loading must fail after cleanup");
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
            startAudioForCleanup(app);
            app.init("Config/settings.json");
            require(app.isRunning, "Repeated initialization failed");
            require(!app.audio.playMusic("cleanup.music") &&
                    app.audio.playSound("cleanup.sound") == -1,
                    "Repeated initialization retained old audio resources");
            startAudioForCleanup(app);
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
        std::printf("PASS: 300 scene transitions, deferred scene changes/closure, "
                    "UI/text/PNG/font updates, repeated init, "
                    "WAV/OGG playback, pause/resume, volume/mute, exhausted channels, "
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
