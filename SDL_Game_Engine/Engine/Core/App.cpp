#include "./App.h"

#include "../Vendor/SDL2/include/SDL2/SDL.h"
#include "../Vendor/SDL2/include/SDL2/SDL_image.h"
#include "../Vendor/SDL2/include/SDL2/SDL_ttf.h"
#include "../Vendor/SDL2/include/SDL2/SDL_mixer.h"

#include "iostream"
#include <algorithm>
#include <cstring>

#include "./Config.h"
#include "../Scenes/SceneManager.h"


SDL_Renderer* App::renderer = nullptr;
SDL_Event App::event;
float App::deltaTime = 0.0f;
SceneManager* App::sceneManager = nullptr;

App::~App() {
    clean();
}

void App::init(const char* configPath) {
    clean();
    AppConfig cfg;

    try {
        cfg = loadConfig(configPath);
    } catch (const std::exception& e) {
        std::cerr << "Blad ladowania konfiguracji: " << e.what() << std::endl;
        isRunning = false;
        return;
    }

    this->title = cfg.window.title;
    settings.fpsLimit = std::max(1, cfg.game.fps);
    settings.fullscreen = cfg.window.fullscreen;
    settings.width = cfg.window.width;
    settings.height = cfg.window.height;

    int flags = 0;
    if(cfg.window.fullscreen){
        flags = SDL_WINDOW_FULLSCREEN;
    }

    int xpos = SDL_WINDOWPOS_CENTERED;
    int ypos = SDL_WINDOWPOS_CENTERED;

    SDL_setenv("SDL_AUDIODRIVER", "directsound", 0);

    // SDL_Init can fail after initializing some subsystems.
    sdlInitialized = true;
    if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "SDL_Init error: " << SDL_GetError() << std::endl;
        clean();
        return;
    }
    std::cout << "SDL works!" << std::endl;

    window = SDL_CreateWindow(cfg.window.title.c_str(), xpos, ypos, cfg.window.width, cfg.window.height, flags);
    if(!window) {
        std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << std::endl;
        clean();
        return;
    }
    std::cout << "Window Created!" << std::endl;

    const char* videoDriver = SDL_GetCurrentVideoDriver();
    const bool headless = videoDriver &&
        (std::strcmp(videoDriver, "dummy") == 0 || std::strcmp(videoDriver, "offscreen") == 0);
    // Headless windows cannot use Direct3D; avoid failed hardware initialization.
    renderer = SDL_CreateRenderer(window, -1,
        headless ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) {
        std::cerr << "SDL_CreateRenderer error: " << SDL_GetError() << std::endl;
        clean();
        return;
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    std::cout << "Renderer Created!" << std::endl;
    syncDisplaySettings();

    if(TTF_Init() == -1) {
        std::cerr << "TTF_Init error: " << TTF_GetError() << std::endl;
        clean();
        return;
    }
    ttfInitialized = true;
    std::cout << "TTF Initialized!" << std::endl;

    imageInitialized = true;
    if((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        std::cerr << "IMG_Init error: " << IMG_GetError() << std::endl;
        clean();
        return;
    }

    mixerInitialized = true;
    // Open the device first: Mix_CloseAudio then frees codec decoder lists
    // even if the subsequent codec initialization fails.
    if(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        std::cerr << "Mix_OpenAudio error: " << Mix_GetError() << std::endl;
        clean();
        return;
    }
    audioOpened = true;
    if((Mix_Init(MIX_INIT_MP3) & MIX_INIT_MP3) == 0) {
        std::cerr << "Mix_Init error: " << Mix_GetError() << std::endl;
        clean();
        return;
    }
    std::cout << "Audio Initialized!" << std::endl;
    applyAudioSettings();

    sceneManager = new SceneManager();
    isRunning = true;
    sceneManager->changeScene(createScene1, *this);
}

void App::update() {
    if (sceneManager && sceneManager->activeScene) {
        sceneManager->activeScene->update(*this);
    }
}

void App::applyAudioSettings() {
    settings.volume = std::clamp(settings.volume, 0, 100);
    const int volume = settings.muted ? 0 : settings.volume * MIX_MAX_VOLUME / 100;
    Mix_Volume(-1, volume);
    Mix_VolumeMusic(volume);
}

void App::syncDisplaySettings() {
    if(!window) return;
    SDL_GetWindowSize(window, &settings.width, &settings.height);
    settings.fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    if(renderer) {
        SDL_RenderSetLogicalSize(renderer, std::max(1024, settings.width),
                                  std::max(720, settings.height));
    }
}

std::vector<WindowResolution> App::getAvailableResolutions() const {
    std::vector<WindowResolution> resolutions;
    if(!window) return resolutions;

    const auto add = [&](int width, int height) {
        if(width <= 0 || height <= 0) return;
        const auto duplicate = std::find_if(resolutions.begin(), resolutions.end(),
            [=](const WindowResolution& resolution) {
                return resolution.width == width && resolution.height == height;
            });
        if(duplicate == resolutions.end()) resolutions.push_back({width, height});
    };
    const int display = SDL_GetWindowDisplayIndex(window);
    SDL_DisplayMode desktop = {};
    if(display >= 0) {
        SDL_GetDesktopDisplayMode(display, &desktop);
        const int modeCount = SDL_GetNumDisplayModes(display);
        for(int i = 0; i < modeCount; ++i) {
            SDL_DisplayMode mode;
            if(SDL_GetDisplayMode(display, i, &mode) == 0) add(mode.w, mode.h);
        }
        add(desktop.w, desktop.h);
    }

    if((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0) {
        for(const WindowResolution resolution : {
                WindowResolution{800,600}, {1024,768}, {1280,720}, {1366,768},
                {1440,900}, {1600,900}, {1920,1080}, {2560,1440}, {3840,2160}}) {
            if((desktop.w <= 0 || resolution.width <= desktop.w) &&
               (desktop.h <= 0 || resolution.height <= desktop.h))
                add(resolution.width, resolution.height);
        }
    }

    const WindowSize current = getWindowSize();
    add(current.wight, current.height);
    std::sort(resolutions.begin(), resolutions.end(),
        [](const WindowResolution& first, const WindowResolution& second) {
            if(first.width != second.width) return first.width < second.width;
            return first.height < second.height;
        });
    return resolutions;
}

bool App::setFullscreen(bool enabled) {
    if(!window) return false;
    syncDisplaySettings();
    if(settings.fullscreen == enabled) return true;

    SDL_DisplayMode previousMode = {};
    const bool havePreviousMode = SDL_GetWindowDisplayMode(window, &previousMode) == 0;
    if(enabled) {
        const int display = SDL_GetWindowDisplayIndex(window);
        SDL_DisplayMode desired = {}, closest = {};
        desired.w = settings.width;
        desired.h = settings.height;
        if(display < 0 ||
           (!SDL_GetClosestDisplayMode(display, &desired, &closest) &&
            SDL_GetDesktopDisplayMode(display, &closest) != 0)) {
            std::cerr << "Cannot find a fullscreen display mode: " << SDL_GetError() << '\n';
            return false;
        }
        if(SDL_SetWindowDisplayMode(window, &closest) != 0) {
            std::cerr << "Cannot set fullscreen resolution: " << SDL_GetError() << '\n';
            SDL_SetWindowDisplayMode(window, havePreviousMode ? &previousMode : nullptr);
            return false;
        }
    }

    if(SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN : 0) != 0) {
        std::cerr << "Cannot change fullscreen mode: " << SDL_GetError() << '\n';
        SDL_SetWindowDisplayMode(window, havePreviousMode ? &previousMode : nullptr);
        syncDisplaySettings();
        return false;
    }
    syncDisplaySettings();
    return settings.fullscreen == enabled;
}

bool App::setResolution(int width, int height) {
    if(!window || width <= 0 || height <= 0) return false;
    syncDisplaySettings();
    if(settings.width == width && settings.height == height) return true;
    const int previousWidth = settings.width, previousHeight = settings.height;
    SDL_DisplayMode previousMode = {};
    const auto restoreFullscreenMode = [&]() {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowDisplayMode(window, &previousMode);
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    };

    if(settings.fullscreen) {
        const int display = SDL_GetWindowDisplayIndex(window);
        SDL_DisplayMode desired = {}, closest = {};
        desired.w = width;
        desired.h = height;
        if(display < 0 || !SDL_GetClosestDisplayMode(display, &desired, &closest) ||
           closest.w != width || closest.h != height ||
           SDL_GetWindowDisplayMode(window, &previousMode) != 0) {
            std::cerr << "Unsupported fullscreen resolution: " << width << 'x' << height << '\n';
            return false;
        }
        if(SDL_SetWindowFullscreen(window, 0) != 0) {
            std::cerr << "Cannot leave fullscreen to change resolution: " << SDL_GetError() << '\n';
            syncDisplaySettings();
            return false;
        }
        if(SDL_SetWindowDisplayMode(window, &closest) != 0 ||
           SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) != 0) {
            std::cerr << "Cannot change fullscreen resolution: " << SDL_GetError() << '\n';
            restoreFullscreenMode();
            syncDisplaySettings();
            return false;
        }
    } else {
        SDL_SetWindowSize(window, width, height);
    }

    const WindowSize actual = getWindowSize();
    if(actual.wight != width || actual.height != height) {
        std::cerr << "Window did not accept resolution " << width << 'x' << height << '\n';
        if(settings.fullscreen) restoreFullscreenMode();
        else SDL_SetWindowSize(window, previousWidth, previousHeight);
        syncDisplaySettings();
        return false;
    }
    syncDisplaySettings();
    return true;
}

void App::handleEvents() {
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                isRunning = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_F1) {
                    devMode = !devMode;
                }
                break;
            case SDL_WINDOWEVENT:
                if(event.window.windowID == SDL_GetWindowID(window) &&
                   (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    event.window.event == SDL_WINDOWEVENT_RESIZED))
                    syncDisplaySettings();
                break;
            default: ;
        }

        if(sceneManager) sceneManager->handleEvents(*this, event);
    }
}

void App::render() {
    if(!renderer) return;
    if (sceneManager && sceneManager->activeScene) {
        sceneManager->activeScene->render(*this);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }

    SDL_RenderPresent(renderer);
}

void App::clean() {
    const bool hadResources = sceneManager || renderer || window || sdlInitialized ||
        ttfInitialized || imageInitialized || mixerInitialized || audioOpened;
    isRunning = false;

    // Scene destructors release textures and fonts while SDL and TTF are alive.
    if(sceneManager) {
        sceneManager->closeScene(*this);
        delete sceneManager;
        sceneManager = nullptr;
    }
    if(renderer) SDL_DestroyRenderer(renderer);
    renderer = nullptr;
    if(window) SDL_DestroyWindow(window);
    window = nullptr;

    if(audioOpened) Mix_CloseAudio();
    audioOpened = false;
    if(mixerInitialized) Mix_Quit();
    mixerInitialized = false;
    if(imageInitialized) IMG_Quit();
    imageInitialized = false;
    if(ttfInitialized) TTF_Quit();
    ttfInitialized = false;
    if(sdlInitialized) {
        SDL_Quit();
        // Failed initialization can leave this thread's SDL error storage alive.
        SDL_TLSCleanup();
    }
    sdlInitialized = false;

    if(hadResources) std::cout << "Game closed!" << std::endl;
}
