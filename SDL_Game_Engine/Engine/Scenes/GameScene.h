#pragma once

#include "Scene.h"
#include "../ECS/ECS.h"
#include "../ECS/AnimationSystem.h"
#include "../Renderer/ColliderDebugRenderer.h"
#include "../Renderer/SpriteRenderer.h"
#include "../UI/UILayer.h"

class GameScene : public Scene {
protected:
    ecs::Registry world;

    virtual void updateWorld(App& app) {}
    virtual void renderWorld(App& app) {}

public:
    UILayer* uiLayer = nullptr;

    void handleEvents(App& app, SDL_Event &event) override {};
    void update(App& app) override {
        updateWorld(app);
        updateAnimations(world, App::deltaTime);
    }
    void render(App& app) override {
        if (!app.renderer) return;
        SDL_SetRenderDrawColor(app.renderer, 24, 28, 36, 255);
        SDL_RenderClear(app.renderer);
        renderWorld(app);
        renderSprites(world, app.renderer);
        renderColliderDebug(world, app.renderer, app.devMode);
        if (uiLayer) uiLayer->render();
    }
    void onEnter(App& app) override {};
    void onExit(App& app) override {};
};
