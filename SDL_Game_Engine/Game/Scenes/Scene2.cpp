#include "../../Engine/Scenes/GameScene.h"
#include "../../Engine/ECS/Components/SpriteComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/ECS/Components/VelocityComponent.h"
#include "../../Engine/Scenes/SceneManager.h"
#include "../../Engine/World/Map.h"

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

        cameraEntity = world.createEntity();
        auto& view = world.addComponent<CameraComponent>(cameraEntity);
        view.size = {width, static_cast<float>(screen.height)};

        view.position = map.isLoaded()
            ? map.position + math::Vector2{1350.0f, 1320.0f} * map.scale - view.size * 0.5f
            : math::Vector2{};
        view.following = false;
        view.bounded = map.isLoaded();
        view.bounds = {map.position.x, map.position.y, mapSize.x, mapSize.y};
        updateCameras(world);
    }

protected:
    void renderWorld(App& app) override {
        map.draw(app.renderer, camera());
    }

    void updateWorld(App& app) override {
        const float dt = App::deltaTime;

        world.forEach<TransformComponent, VelocityComponent>(
                [dt](ecs::Entity, TransformComponent& transform,
                     const VelocityComponent& velocity) {
                    transform.position += velocity.total() * dt;
                });

        const auto* keys = SDL_GetKeyboardState(nullptr);
        if (auto* view = camera()) {
            view->move({static_cast<float>(keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT]),
                        static_cast<float>(keys[SDL_SCANCODE_DOWN] - keys[SDL_SCANCODE_UP])}, dt);
        }
    }

    void handleEvents(App& app, SDL_Event &event) override {
        if (event.type == SDL_KEYDOWN) {

            if (event.key.keysym.sym == SDLK_f && !event.key.repeat) {
                if (auto* view = camera()) view->following = !view->following;
            }

            if (event.key.keysym.sym == SDLK_ESCAPE) {
                app.sceneManager->changeScene(createScene1, app);
            }
        }
    }
};

Scene* createScene2() {
    return new Scene2();
}