#pragma once
#include <string>
#include "../Vendor/SDL2/include/SDL2/SDL.h"

class SceneManager;

struct WindowSize {
    int wight;
    int height;
};

class App {
public:
    App() {};
    ~App() {};

    std::string title;
    static float deltaTime;
    bool isRunning = false;
    bool devMode = false;

    SDL_Window *window = nullptr;
    static SDL_Renderer *renderer;
    static SDL_Event event;

    static SceneManager* sceneManager;

    WindowSize getWindowSize() {
        int w,h;
        SDL_GetWindowSize(this->window,&w, &h);
        return {w,h};
    };

//    static Audio audio;

    void init(const char* configPath);
    void update();
    void handleEvents();
    void render();
    void clean();
private:
};