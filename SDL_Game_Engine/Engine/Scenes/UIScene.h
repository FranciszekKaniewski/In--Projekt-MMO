#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <functional>

#include "./Scene.h"
#include "../Core/App.h"
#include "../UI/UILayer.h"

class UIScene : public Scene {
public:
    SDL_Color color;
    UILayer* uiLayer = nullptr;

    UIScene(SDL_Color color = {200,200,200}) : color(color) {};
    virtual ~UIScene() = default;

    void handleEvents(App& app, SDL_Event &event) override {
        if(uiLayer) uiLayer->handleEvents();
    }
    void update(App& app) override {}

    void render(App& app) override {
        SDL_SetRenderDrawColor(app.renderer, color.r, color.g, color.b, 255);
        SDL_RenderClear(app.renderer);

        if(uiLayer) uiLayer->render();
    }

    void onEnter(App& app) override {}
    void onExit(App& app) override {}

protected:
    void clear() {}
};