#include "./App.h"

#include "../Vendor/SDL2/include/SDL2/SDL.h"
#include "../Vendor/SDL2/include/SDL2/SDL_image.h"
#include "../Vendor/SDL2/include/SDL2/SDL_ttf.h"
#include "../Vendor/SDL2/include/SDL2/SDL_mixer.h"

#include "iostream"

#include "./Config.h"
#include "../Scenes/SceneManager.h"


SDL_Renderer* App::renderer = nullptr;
SDL_Event App::event;
float App::deltaTime = 0.0f;
SceneManager* App::sceneManager = new SceneManager();

void App::init(const char* configPath) {
    AppConfig cfg;

    try {
        cfg = loadConfig(configPath);
    } catch (const std::exception& e) {
        std::cerr << "Blad ladowania konfiguracji: " << e.what() << std::endl;
        isRunning = false;
        return;
    }

    this->title = cfg.window.title;

    int flags = 0;
    if(cfg.window.fullscreen){
        flags = SDL_WINDOW_FULLSCREEN;
    }

    int xpos = SDL_WINDOWPOS_CENTERED;
    int ypos = SDL_WINDOWPOS_CENTERED;

    SDL_setenv("SDL_AUDIODRIVER", "directsound", 1);

    if(SDL_Init(SDL_INIT_EVERYTHING) == 0){
        std::cout << "SDL works!" << std::endl;

        window = SDL_CreateWindow(cfg.window.title.c_str(), xpos, ypos, cfg.window.width, cfg.window.height, flags);
        if(window){
            std::cout << "Window Created!" << std::endl;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
        if(renderer){
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            std::cout << "Renderer Created!" << std::endl;
        }

        if (TTF_Init() == -1) {
            std::cerr << "TTF_Init error: " << TTF_GetError() << std::endl;
            return;
        }
        std::cout << "TFF Initialized!" << std::endl;

        if (Mix_Init(MIX_INIT_MP3) == 0) {
            std::cerr << "Mix_Init Error: " << Mix_GetError() << std::endl;
            return;
        }
        if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
            std::cerr << "Mix_OpenAudio Error: " << Mix_GetError() << std::endl;
            return;
        }
        std::cout << "Audio Initialized!" << std::endl;

        isRunning = true;
        sceneManager->changeScene(createScene1, *this);
    } else {
        isRunning = false;
    }
}

void App::update() {
    if (sceneManager->activeScene) {
        sceneManager->activeScene->update(*this);
    }
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
            default: ;
        }

        if (sceneManager->activeScene) {
            sceneManager->activeScene->handleEvents(*this, event);
        }
    }
}

void App::render() {
    if (sceneManager->activeScene) {
        sceneManager->activeScene->render(*this);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }

    SDL_RenderPresent(renderer);
}

void App::clean() {
    sceneManager->closeScene(*this);
    delete sceneManager;

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    std::cout << "Game closed!" << std::endl;
}