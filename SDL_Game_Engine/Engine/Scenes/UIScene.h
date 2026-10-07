#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <functional>

#include "./Scene.h"
#include "../Core/App.h"
#include "../UI/UILayer.h"
#include "../UI/UIBackground.h"

class UIScene : public Scene {
private:
    UIBackground background;

public:
    SDL_Color color;
    UILayer* uiLayer = nullptr;

    UIScene(SDL_Color color = {200,200,200}, const std::string& backgroundImage = "") : color(color) {
        if (!backgroundImage.empty()) setBackgroundImage(backgroundImage);
    };
    ~UIScene() override {
        delete uiLayer;
    }

    bool setBackgroundImage(const std::string& fileName) {
        return background.setImage(fileName.c_str());
    }

    void clearBackgroundImage() {
        background.clearImage();
    }

    void setBackgroundOpacity(Uint8 alpha) {
        background.setOpacity(alpha);
    }

    void handleEvents(App& app, SDL_Event &event) override {
        handleUIEvents(event);
    }
    void update(App& app) override {}

    void render(App& app) override {
        if (!app.renderer) return;

        SDL_SetRenderDrawColor(app.renderer, color.r, color.g, color.b, 255);
        SDL_RenderClear(app.renderer);

        int width = 0, height = 0;
        SDL_RenderGetLogicalSize(app.renderer, &width, &height);
        if (width <= 0 || height <= 0)
            SDL_GetRendererOutputSize(app.renderer, &width, &height);
        background.draw({0,0,width,height}, {0,0,0,0}, 0);

        if(uiLayer) uiLayer->render();
    }

    void onEnter(App& app) override {}
    void onExit(App& app) override {
        delete uiLayer;
        uiLayer = nullptr;
    }

protected:
    bool handleUIEvents(const SDL_Event& event) {
        return uiLayer && uiLayer->handleEvents(event);
    }

    void clear() {}
};
