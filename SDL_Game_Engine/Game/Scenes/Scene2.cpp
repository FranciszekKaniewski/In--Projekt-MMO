#include "../../Engine/Scenes/GameScene.h"
#include "../../Engine/ECS/Components/AnimationComponent.h"
#include "../../Engine/ECS/Components/ColliderComponent.h"
#include "../../Engine/ECS/Components/SpriteComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/ECS/Components/VelocityComponent.h"
#include "../../Engine/Renderer/TextureManager.h"
#include "../../Engine/Scenes/SceneManager.h"

#include <cmath>

class Scene2 : public GameScene {
private:
    ecs::Entity square{};
    ecs::Entity polygon{};
    ecs::Entity circle{};

    float angle = 0.0f;
    float angularSpeed = 0.6f;
    float movementRadius = 100.0f;

public:
    Scene2() : GameScene() {}

    void onEnter(App& app) override {
        std::cout << "Entering Game Scene !" << std::endl;

        angle = 0.0f;
        const auto screen = app.getUISize();
        const float width = static_cast<float>(screen.wight);
        const float centerY = static_cast<float>(screen.height) / 2.0f;

        world.clear();
        square = world.createEntity();
        world.addComponent<TransformComponent>(square, math::Vector2{width / 6.0f, centerY});
        world.addComponent<VelocityComponent>(square);
        world.addComponent<ColliderComponent>(square, ColliderComponent::square(128.0f));

        polygon = world.createEntity();
        world.addComponent<TransformComponent>(polygon, math::Vector2{width / 2.0f, centerY});
        world.addComponent<VelocityComponent>(polygon);
        world.addComponent<ColliderComponent>(polygon, ColliderComponent::polygon({
            {-80.0f, 20.0f}, {-50.0f, -60.0f}, {40.0f, -70.0f},
            {90.0f, 10.0f}, {0.0f, 80.0f}
        }));

        circle = world.createEntity();
        world.addComponent<TransformComponent>(circle,math::Vector2{width * 5.0f / 6.0f + movementRadius, centerY});
        world.addComponent<VelocityComponent>(circle);
        world.addComponent<ColliderComponent>(circle, ColliderComponent::circle(64.0f));

        const auto bluePotionTexture = TextureManager::LoadSharedTexture("Assets/Graphics/Potion_blue.png");
        const auto redPotionTexture = TextureManager::LoadSharedTexture("Assets/Graphics/Potion_red.png");
        const auto greenPotionTexture = TextureManager::LoadSharedTexture("Assets/Graphics/Potion_green.png");

        const auto addPotionSprite = [&](ecs::Entity entity, char color = 'b') {
            std::map<char, std::shared_ptr<SDL_Texture>> map = {
                    {'b',bluePotionTexture},{'r',redPotionTexture},{'g',greenPotionTexture}
            };
            auto& sprite = world.addComponent<SpriteComponent>(entity);

            sprite.texture = map[color];
            sprite.source = {0, 0, 12, 17};
            sprite.size = {128.0f, 180.0f};
            sprite.offset = {};

            SDL_SetTextureScaleMode(map[color].get(), SDL_ScaleModeNearest);

            auto& animation = world.addComponent<AnimationComponent>(entity);
            animation.frameCount = 3;
            animation.frameDuration = 0.3;
            animation.loop = true;
        };

        addPotionSprite(square);
        addPotionSprite(circle,'r');
        addPotionSprite(polygon,'g');
    }

protected:
    void updateWorld(App& app) override {
        const float dt = App::deltaTime;

        angle += angularSpeed * dt;

        auto& squareVelocity = world.getComponent<VelocityComponent>(square);
        squareVelocity.vectors["movement"] = {
                0.0f,
                std::cos(angle) * movementRadius * angularSpeed
        };

        auto& polygonVelocity = world.getComponent<VelocityComponent>(polygon);
        polygonVelocity.vectors["movement"] = {
                std::cos(angle) * movementRadius * angularSpeed,
                0.0f
        };

        auto& circleVelocity = world.getComponent<VelocityComponent>(circle);
        circleVelocity.vectors["movement"] = {
                -std::sin(angle) * movementRadius * angularSpeed,
                std::cos(angle) * movementRadius * angularSpeed
        };

        world.forEach<TransformComponent, VelocityComponent>(
                [dt](ecs::Entity, TransformComponent& transform,
                     const VelocityComponent& velocity) {
                    transform.position += velocity.total() * dt;
                });
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