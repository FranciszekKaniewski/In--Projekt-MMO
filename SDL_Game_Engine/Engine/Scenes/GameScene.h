#pragma once

#include "Scene.h"
#include "../ECS/ECS.h"
#include "../UI/UILayer.h"

class GameScene : public Scene {
protected:
    ecs::Registry world;

public:
    UILayer* uiLayer = nullptr;

    void handleEvents(App& app, SDL_Event &event) override {};
    void update(App& app) override {};
    void render(App& app) override {};
    void onEnter(App& app) override {};
    void onExit(App& app) override {};
};