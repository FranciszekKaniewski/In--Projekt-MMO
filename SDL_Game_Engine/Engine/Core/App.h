#pragma once
#include <string>
#include <vector>
#include "../Vendor/SDL2/include/SDL2/SDL.h"

class SceneManager;

struct WindowSize {
    int wight;
    int height;
};

struct GameSettings {
    int volume = 100;
    bool muted = false;
    int fpsLimit = 60;
    bool fullscreen = false;
    int width = 1280;
    int height = 720;
};

struct WindowResolution {
    int width;
    int height;
};

class App {
public:
    App() = default;
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    std::string title;
    static float deltaTime;
    bool isRunning = false;
    bool devMode = false;
    GameSettings settings;

    SDL_Window *window = nullptr;
    static SDL_Renderer *renderer;
    static SDL_Event event;

    static SceneManager* sceneManager;

    WindowSize getWindowSize() const {
        int w = 0, h = 0;
        SDL_GetWindowSize(this->window,&w, &h);
        return {w,h};
    };

    WindowSize getUISize() const {
        int width = 0, height = 0;
        if(renderer) SDL_RenderGetLogicalSize(renderer, &width, &height);
        if(width > 0 && height > 0) return {width, height};
        return getWindowSize();
    }

//    static Audio audio;

    void init(const char* configPath);
    void update();
    void handleEvents();
    void render();
    void clean();
    void applyAudioSettings();
    void syncDisplaySettings();
    bool setFullscreen(bool enabled);
    bool setResolution(int width, int height);
    std::vector<WindowResolution> getAvailableResolutions() const;
private:
    bool sdlInitialized = false;
    bool ttfInitialized = false;
    bool imageInitialized = false;
    bool mixerInitialized = false;
    bool audioOpened = false;
};
