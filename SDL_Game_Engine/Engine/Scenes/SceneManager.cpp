#include "SceneManager.h"
#include <utility>

void SceneManager::changeScene(std::function<Scene*()> factory, App& app) {
    if (handlingEvents) {
        pendingSceneFactory = std::move(factory);
        hasPendingSceneChange = true;
        return;
    }

    closeScene(app);

    if (factory) {
        activeScene = factory();
        if (activeScene) {
            activeScene->onEnter(app);
        }
    }
}

void SceneManager::closeScene(App& app) {
    if (activeScene != nullptr) {
        activeScene->onExit(app);
        delete activeScene;
        activeScene = nullptr;
    }
}

void SceneManager::handleEvents(App& app, SDL_Event& event) {
    if (!activeScene) return;

    // Scene changes may delete UI elements; finish dispatch before applying them.
    handlingEvents = true;
    activeScene->handleEvents(app, event);
    handlingEvents = false;

    if (hasPendingSceneChange) {
        auto factory = std::move(pendingSceneFactory);
        hasPendingSceneChange = false;
        changeScene(std::move(factory), app);
    }
}
