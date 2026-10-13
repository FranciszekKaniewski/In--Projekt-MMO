#include "SceneManager.h"
#include <memory>
#include <utility>

namespace {
struct ScopedFlag {
    bool& flag;
    bool previous;

    explicit ScopedFlag(bool& flag) : flag(flag), previous(std::exchange(flag, true)) {}
    ~ScopedFlag() { flag = previous; }
};
}

void SceneManager::changeScene(std::function<Scene*()> factory, App& app) {
    if (processingScene || changingScene) {
        pendingSceneFactory = std::move(factory);
        hasPendingSceneChange = true;
        return;
    }

    pendingSceneFactory = {};
    hasPendingSceneChange = false;
    const ScopedFlag guard(changingScene);
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
    // Lifecycle callbacks may queue a transition even when no scene is active.
    applyPendingSceneChange(app);
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
    if (!hasPendingSceneChange || processingScene || changingScene) return;

    auto factory = std::move(pendingSceneFactory);
    hasPendingSceneChange = false;
    changeScene(std::move(factory), app);
}
