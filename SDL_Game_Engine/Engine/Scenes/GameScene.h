#pragma once

#include "Scene.h"
#include "../ECS/ECS.h"
#include "../ECS/AnimationSystem.h"
#include "../ECS/CameraSystem.h"
#include "../ECS/DirectionalAnimationSystem.h"
#include "../ECS/MovementSystem.h"
#include "../ECS/Components/CameraComponent.h"
#include "../Input/KeyboardInputSystem.h"
#include "../Renderer/ColliderDebugRenderer.h"
#include "../Renderer/SpriteRenderer.h"
#include "../UI/UILayer.h"
#include "../UI/ChatOverlay.h"
#include <memory>

class GameScene : public Scene {
protected:
    ecs::Registry world;
    ecs::Entity cameraEntity{};
    ChatSession chat;
    std::unique_ptr<ChatOverlay> chatOverlay;

    CameraComponent* camera() { return world.tryGetComponent<CameraComponent>(cameraEntity); }

    virtual void updateWorld(App& app) {}
    virtual void renderWorld(App& app) {}
    virtual void handleWorldEvents(App& app, SDL_Event& event) {}

    void enableChat(ecs::Entity localSpeaker) {
        chat.reset(localSpeaker);
        chatOverlay = std::make_unique<ChatOverlay>(world, chat);
        receiveServerMessage(u8"Hello! Press Enter, to write a message.");
    }

public:
    UILayer* uiLayer = nullptr;

    ChatSession& chatSession() { return chat; }
    bool receiveChatMessage(ChatMessage message) { return chat.receive(world, std::move(message)); }
    bool receiveServerMessage(const std::string& text) {
        return receiveChatMessage({ChatSource::Server, {}, "Server", text});
    }

    void handleEvents(App& app, SDL_Event &event) override {
        if (chatOverlay && chatOverlay->handleEvent(event)) return;
        if (uiLayer && uiLayer->handleEvents(event)) return;
        handleWorldEvents(app, event);
    }
    void update(App& app) override {
        updateKeyboardInput(world, !chatOverlay || !chatOverlay->isFocused());
        updateWorld(app);
        updateMovement(world, App::deltaTime);
        updateDirectionalAnimations(world);
        updateAnimations(world, App::deltaTime);
        updateCameras(world);
        chat.update(world, App::deltaTime);
    }
    void render(App& app) override {
        if (!app.renderer) return;
        SDL_SetRenderDrawColor(app.renderer, 24, 28, 36, 255);
        SDL_RenderClear(app.renderer);
        renderWorld(app);
        renderSprites(world, app.renderer, camera());
        renderColliderDebug(world, app.renderer, app.devMode, camera());
        if (chatOverlay) chatOverlay->render(camera(), app.getUISize());
        if (uiLayer) uiLayer->render();
    }
    void onEnter(App& app) override {};
    void onExit(App& app) override {};
};
