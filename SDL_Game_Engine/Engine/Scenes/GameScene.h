#pragma once

#include "Scene.h"
#include "../ECS/ECS.h"
#include "../Renderer/ColliderDebugRenderer.h"
#include "../UI/UILayer.h"

class GameScene : public Scene {
protected:
    ecs::Registry world;

    virtual void renderWorld(App& app) {}

public:
    UILayer* uiLayer = nullptr;

    void handleEvents(App& app, SDL_Event &event) override {};
    void update(App& app) override {};
    void render(App& app) override {
        if (!app.renderer) return;
        SDL_SetRenderDrawColor(app.renderer, 24, 28, 36, 255);
        SDL_RenderClear(app.renderer);
        renderWorld(app);
        renderColliderDebug(world, app.renderer, app.devMode);
        if (uiLayer) uiLayer->render();
    }
    void onEnter(App& app) override {};
    void onExit(App& app) override {};
};