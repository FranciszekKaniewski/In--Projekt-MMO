#pragma once
#include "../Core/App.h"

class Scene {
public:
    virtual ~Scene() = default;

    virtual void handleEvents(App& app, SDL_Event &event) = 0;
    virtual void update(App& app) = 0;
    virtual void render(App& app) = 0;
    virtual void onEnter(App& app) {}
    virtual void onExit(App& app) {}
};