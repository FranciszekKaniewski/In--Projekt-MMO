#include "../../Engine/Scenes/GameScene.h"
#include "../../Engine/ECS/Components/KeyboardControlComponent.h"
#include "../../Engine/ECS/Components/MovementBoundsComponent.h"
#include "../../Engine/ECS/Components/MovementComponent.h"
#include "../../Engine/ECS/Components/MovementInputComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/ECS/Components/VelocityComponent.h"
#include "../../Engine/ECS/Components/ChatSpeakerComponent.h"
#include "../../Engine/Scenes/SceneManager.h"
#include "../../Engine/World/Map.h"
#include "../Entities/Player.h"
#include <algorithm>
#include <chrono>
#include <unordered_map>

class Scene2 : public GameScene {
private:
    Map map;
    CharacterState saved;
    ecs::Entity player;
    std::unordered_map<std::int64_t, ecs::Entity> others;
    RequestId pendingMove = 0;
    int sentX = 0, sentY = 0;
    std::chrono::steady_clock::time_point nextInput;

    void applySnapshot(const std::vector<WorldPlayer>& snapshot) {
        for(auto it = others.begin(); it != others.end();) {
            const auto id = it->first;
            if(std::none_of(snapshot.begin(), snapshot.end(), [id](const auto& state) { return state.id == id; })) {
                world.destroyEntity(it->second);
                it = others.erase(it);
            } else {
                ++it;
            }
        }
        for(const auto& state : snapshot) {
            auto entity = player;
            if(state.id != saved.id) {
                const auto found = others.find(state.id);
                if(found == others.end()) {
                    entity = spawnPlayer(world, {state.x, state.y});
                    if(!entity) continue;
                    world.removeComponent<VelocityComponent>(entity);
                    others.emplace(state.id, entity);
                    world.getComponent<ChatSpeakerComponent>(entity).name = state.name;
                } else {
                    entity = found->second;
                }
            }
            world.getComponent<TransformComponent>(entity).position = {state.x, state.y};
            auto& movement = world.getComponent<MovementComponent>(entity);
            movement.facing = static_cast<FacingDirection>(state.facing);
            movement.moving = state.moving;
        }
    }

public:
    Scene2(Map loaded, CharacterState character) : map(std::move(loaded)), saved(std::move(character)) {}

    void onEnter(App& app) override {
        std::cout << "Entering Game Scene !" << std::endl;

        const auto screen = app.getUISize();
        const float width = static_cast<float>(screen.wight);
        map.scale = 1.0f;
        map.position = {};
        const auto mapSize = map.worldSize();

        world.clear();

        player = spawnPlayer(world, {saved.x, saved.y});
        if (!player) return;
        world.getComponent<ChatSpeakerComponent>(player).name = saved.name;
        world.getComponent<MovementComponent>(player).facing = static_cast<FacingDirection>(saved.facing);
        world.addComponent<KeyboardControlComponent>(player);
        if(saved.id) {
            // Input and animation still use ECS. The server owns the position.
            world.removeComponent<VelocityComponent>(player);
        } else {
            auto& bounds = world.addComponent<MovementBoundsComponent>(player);
            bounds.minimum = map.position + math::Vector2{10.0f, 5.0f};
            bounds.maximum = map.position + mapSize - math::Vector2{10.0f, 5.0f};
        }

        // Camera and character belong to the same entity; this scene only composes them.
        cameraEntity = player;
        auto& view = world.addComponent<CameraComponent>(cameraEntity);
        view.size = {width, static_cast<float>(screen.height)};
        view.target = player;
        view.following = true;
        view.bounded = true;
        view.bounds = {map.position.x, map.position.y, mapSize.x, mapSize.y};
        updateCameras(world);
        enableChat(player);
    }

protected:
    void updateWorld(App& app) override {
        if(!saved.id || !app.serverClient || !player) return;
        while(auto reply = app.serverClient->poll()) {
            if(reply->request == pendingMove) pendingMove = 0;
            if(reply->type == ServerReply::Type::Error) {
                if(!app.serverClient->isAuthenticated()) {
                    app.loginMessage = serverErrorText(reply->error);
                    App::sceneManager->changeScene(createScene1, app);
                    return;
                }
                if(reply->error == "world_required") {
                    App::sceneManager->changeScene(createCharactersScene, app);
                    return;
                }
                nextInput = {};
            }
        }
        if(auto snapshot = app.serverClient->pollWorld()) applySnapshot(*snapshot);
        if(pendingMove) return;
        const auto direction = world.getComponent<MovementInputComponent>(player).direction;
        const int dx = static_cast<int>(direction.x), dy = static_cast<int>(direction.y);
        const auto now = std::chrono::steady_clock::now();
        if(dx != sentX || dy != sentY || ((dx || dy) && now >= nextInput)) {
            pendingMove = app.serverClient->moveInput(dx, dy);
            sentX = dx;
            sentY = dy;
            nextInput = now + std::chrono::milliseconds(250);
        }
    }

    void renderWorld(App& app) override {
        map.draw(app.renderer, camera());
    }

    void handleWorldEvents(App& app, SDL_Event &event) override {
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE && !event.key.repeat) {
                if(saved.id && app.serverClient && app.serverClient->isAuthenticated()) app.serverClient->leaveWorld();
                app.sceneManager->changeScene(
                    app.serverClient && app.serverClient->isAuthenticated() ? createCharactersScene : createScene1, app);
            }
        }
    }
};

Scene* createNetworkGameScene(Map map, CharacterState character) {
    return new Scene2(std::move(map), std::move(character));
}
