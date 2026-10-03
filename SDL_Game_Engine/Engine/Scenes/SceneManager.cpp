#include "SceneManager.h"

void SceneManager::changeScene(std::function<Scene*()> factory, App& app) {
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