#include "../../Engine/Scenes/GameScene.h"
#include "../../Engine/ECS/Components/KeyboardControlComponent.h"
#include "../../Engine/ECS/Components/MovementBoundsComponent.h"
#include "../../Engine/Scenes/SceneManager.h"
#include "../../Engine/World/Map.h"
#include "../Entities/Player.h"

class Scene2 : public GameScene {
private:
    Map map;

public:
    Scene2() : GameScene() {}

    void onEnter(App& app) override {
        std::cout << "Entering Game Scene !" << std::endl;

        const auto screen = app.getUISize();
        const float width = static_cast<float>(screen.wight);
        const float centerY = static_cast<float>(screen.height) / 2.0f;

        map.scale = 1.0f;
        map.position = {};
        if (!map.load("Assets/Maps/Map_expanded.json")) {
            std::cerr << map.error() << std::endl;
        }
        const auto mapSize = map.isLoaded() ? map.worldSize() : math::Vector2{width, centerY * 2.0f};

        world.clear();

        const auto spawn = map.isLoaded()
            ? map.position + math::Vector2{1350.0f, 1320.0f} * map.scale
            : math::Vector2{width * 0.5f, centerY};
        const auto player = spawnPlayer(world, spawn);
        if (!player) return;
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
    }

protected:
    void renderWorld(App& app) override {
        map.draw(app.renderer, camera());
    }

    void handleEvents(App& app, SDL_Event &event) override {
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                app.sceneManager->changeScene(createScene1, app);
            }
        }
    }
};

Scene* createScene2() {
    return new Scene2();
}
