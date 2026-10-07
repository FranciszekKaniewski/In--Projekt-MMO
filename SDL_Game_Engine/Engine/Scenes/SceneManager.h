#pragma once
#include <functional>
#include "Scene.h"
#include "SceneList.h"

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() { delete activeScene; }
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    Scene* activeScene = nullptr;

    void changeScene(std::function<Scene*()> factory, App& app);
    void closeScene(App& app);
    void handleEvents(App& app, SDL_Event& event);

private:
    bool handlingEvents = false;
    bool hasPendingSceneChange = false;
    std::function<Scene*()> pendingSceneFactory;
};
