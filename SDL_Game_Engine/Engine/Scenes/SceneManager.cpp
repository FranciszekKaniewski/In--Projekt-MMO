#include "SceneManager.h"
#include <memory>
#include <utility>

void SceneManager::changeScene(std::function<Scene*()> factory, App& app) {
    if (processingScene) {
        pendingSceneFactory = std::move(factory);
        hasPendingSceneChange = true;
        return;
    }

    destroyScene(app);

    if (factory) {
        activeScene = factory();
        if (activeScene) {
            activeScene->onEnter(app);
        }
    }
}

void SceneManager::closeScene(App& app) {
    changeScene({}, app);
}

void SceneManager::destroyScene(App& app) {
    pendingSceneFactory = {};
    hasPendingSceneChange = false;
    std::unique_ptr<Scene> scene(std::exchange(activeScene, nullptr));
    if(scene) scene->onExit(app);
}

void SceneManager::handleEvents(App& app, SDL_Event& event) {
    if (!activeScene) return;

    // Finish the scene method before applying a change that may destroy it.
    processingScene = true;
    activeScene->handleEvents(app, event);
    processingScene = false;

    applyPendingSceneChange(app);
}

void SceneManager::update(App& app) {
    if (!activeScene) return;

    processingScene = true;
    activeScene->update(app);
    processingScene = false;

    applyPendingSceneChange(app);
}

void SceneManager::render(App& app) {
    if (!activeScene) return;

    processingScene = true;
    activeScene->render(app);
    processingScene = false;

    applyPendingSceneChange(app);
}

void SceneManager::applyPendingSceneChange(App& app) {
    if (!hasPendingSceneChange) return;

    auto factory = std::move(pendingSceneFactory);
    hasPendingSceneChange = false;
    changeScene(std::move(factory), app);
}