#include "../../Engine/Scenes/GameScene.h"
#include "../../Engine/ECS/Components/KeyboardControlComponent.h"
#include "../../Engine/ECS/Components/MovementBoundsComponent.h"
#include "../../Engine/ECS/Components/MovementComponent.h"
#include "../../Engine/ECS/Components/ChatSpeakerComponent.h"
#include "../../Engine/Scenes/SceneManager.h"
#include "../../Engine/World/Map.h"
#include "../Entities/Player.h"

class Scene2 : public GameScene {
private:
    Map map;
    std::optional<CharacterState> saved;

public:
    Scene2() = default;
    Scene2(Map loaded, CharacterState character) : map(std::move(loaded)), saved(std::move(character)) {}

    void onEnter(App& app) override {
        std::cout << "Entering Game Scene !" << std::endl;

        const auto screen = app.getUISize();
        const float width = static_cast<float>(screen.wight);
        const float centerY = static_cast<float>(screen.height) / 2.0f;

        map.scale = 1.0f;
        map.position = {};
        if (!map.isLoaded() && !map.load("Assets/Maps/Map_expanded.tmj")) {
            std::cerr << map.error() << std::endl;
        }
        const auto mapSize = map.isLoaded() ? map.worldSize() : math::Vector2{width, centerY * 2.0f};

        world.clear();

        auto spawn = map.isLoaded()
            ? map.position + math::Vector2{1350.0f, 1320.0f} * map.scale
            : math::Vector2{width * 0.5f, centerY};
        if(saved) spawn = {saved->x, saved->y};
        const auto player = spawnPlayer(world, spawn);
        if (!player) return;
        if(saved) {
            world.getComponent<ChatSpeakerComponent>(player).name = saved->name;
            world.getComponent<MovementComponent>(player).facing = static_cast<FacingDirection>(saved->facing);
        }
        world.addComponent<KeyboardControlComponent>(player);
        if (map.isLoaded()) {
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
        view.bounded = map.isLoaded();
        view.bounds = {map.position.x, map.position.y, mapSize.x, mapSize.y};
        updateCameras(world);
        enableChat(player);
    }

    void update(App& app) override {
        GameScene::update(app);
        if(!app.serverClient) return;
        while(auto reply = app.serverClient->poll()) {
            if(reply->type == ServerReply::Type::Error && !app.serverClient->isAuthenticated()) {
                app.loginMessage = serverErrorText(reply->error);
                App::sceneManager->changeScene(createScene1, app);
                return;
            }
        }
    }

protected:
    void renderWorld(App& app) override {
        map.draw(app.renderer, camera());
    }

    void handleWorldEvents(App& app, SDL_Event &event) override {
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                app.sceneManager->changeScene(
                    app.serverClient && app.serverClient->isAuthenticated() ? createCharactersScene : createScene1, app);
            }
        }
    }
};

Scene* createScene2() {
    return new Scene2();
}

Scene* createNetworkGameScene(Map map, CharacterState character) {
    return new Scene2(std::move(map), std::move(character));
}
