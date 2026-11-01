#include <catch_amalgamated.hpp>
#define SDL_MAIN_HANDLED
#include "../../Game/Entities/Player.h"
#include "../../Engine/ECS/AnimationSystem.h"
#include "../../Engine/ECS/DirectionalAnimationSystem.h"
#include "../../Engine/ECS/MovementSystem.h"
#include "../../Engine/ECS/Components/AnimationComponent.h"
#include "../../Engine/ECS/Components/CameraComponent.h"
#include "../../Engine/ECS/Components/DirectionalAnimationComponent.h"
#include "../../Engine/ECS/Components/KeyboardControlComponent.h"
#include "../../Engine/ECS/Components/MovementBoundsComponent.h"
#include "../../Engine/ECS/Components/MovementComponent.h"
#include "../../Engine/ECS/Components/MovementInputComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/ECS/Components/VelocityComponent.h"
#include "../../Engine/Scenes/GameScene.h"
#include "../../Engine/Scenes/SceneList.h"

#include <SDL_image.h>
#include <array>
#include <cmath>
#include <limits>
#include <memory>

namespace {
ecs::Entity addCharacter(ecs::Registry& world, math::Vector2 position = {}) {
    const auto entity = world.createEntity();
    world.addComponent<TransformComponent>(entity, position);
    world.addComponent<VelocityComponent>(entity);
    world.addComponent<MovementInputComponent>(entity);
    world.addComponent<MovementComponent>(entity).speed = 100.0f;
    return entity;
}

TEST_CASE("Movement commands are independent per entity and preserve external velocity",
          "[integration][player][movement]") {
    ecs::Registry world;
    const auto first = addCharacter(world);
    const auto second = addCharacter(world);
    world.getComponent<MovementInputComponent>(first).direction = {1, 1};
    world.getComponent<MovementInputComponent>(second).direction = {-1, 0};
    world.getComponent<VelocityComponent>(second).vectors["wind"] = {0, 20};

    updateMovement(world, 0.5f);
    const auto diagonal = world.getComponent<TransformComponent>(first).position;
    REQUIRE(std::abs(diagonal.length() - 50.0f) < 0.001f);
    REQUIRE(std::abs(diagonal.x - diagonal.y) < 0.001f);
    REQUIRE(world.getComponent<MovementComponent>(first).facing == FacingDirection::SouthEast);
    REQUIRE(world.getComponent<TransformComponent>(second).position.x == -50.0f);
    REQUIRE(world.getComponent<TransformComponent>(second).position.y == 10.0f);

    world.getComponent<MovementInputComponent>(second).direction = {};
    updateMovement(world, 0.5f);
    REQUIRE(world.getComponent<TransformComponent>(second).position.x == -50.0f);
    REQUIRE(world.getComponent<TransformComponent>(second).position.y == 20.0f);
    REQUIRE(world.getComponent<VelocityComponent>(second).vectors.at("movement").length() == 0.0f);
    REQUIRE(world.getComponent<MovementComponent>(second).facing == FacingDirection::West);
}

TEST_CASE("World bounds stop walking without changing facing and invalid time cannot move a player",
          "[integration][player][movement]") {
    ecs::Registry world;
    const auto player = addCharacter(world, {95, 50});
    auto& bounds = world.addComponent<MovementBoundsComponent>(player);
    bounds.minimum = {0, 0};
    bounds.maximum = {100, 100};
    auto& input = world.getComponent<MovementInputComponent>(player);
    auto& movement = world.getComponent<MovementComponent>(player);
    const auto& position = world.getComponent<TransformComponent>(player).position;
    input.direction = {1, 0};
    updateMovement(world, 0.5f);
    REQUIRE(position.x == 100.0f);
    REQUIRE(movement.moving);
    updateMovement(world, 0.5f);
    REQUIRE_FALSE(movement.moving);
    REQUIRE(movement.facing == FacingDirection::East);

    input.direction = {-1, 0};
    for (float dt : {-1.0f, std::numeric_limits<float>::infinity(),
                     std::numeric_limits<float>::quiet_NaN()}) {
        updateMovement(world, dt);
        REQUIRE(position.x == 100.0f);
    }
    input.direction = {std::numeric_limits<float>::quiet_NaN(), 0};
    updateMovement(world, 0.5f);
    REQUIRE(position.x == 100.0f);
    REQUIRE_FALSE(movement.moving);
    input.direction = {-1, 0};
    updateMovement(world, 0.5f);
    REQUIRE(position.x == 50.0f);
    REQUIRE(movement.moving);
}

struct PlayerSDLFixture {
    PlayerSDLFixture() {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
        REQUIRE((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != 0);
    }
    ~PlayerSDLFixture() { IMG_Quit(); SDL_Quit(); }
};

class PlayerCanvas {
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface{nullptr, SDL_FreeSurface};
    const float previousDeltaTime = App::deltaTime;
public:
    App app;
    PlayerCanvas(int width = 256, int height = 256) {
        REQUIRE(App::renderer == nullptr);
        surface.reset(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32));
        REQUIRE(surface != nullptr);
        App::renderer = SDL_CreateSoftwareRenderer(surface.get());
        REQUIRE(App::renderer != nullptr);
        REQUIRE(SDL_RenderSetLogicalSize(App::renderer, width, height) == 0);
    }
    ~PlayerCanvas() { App::deltaTime = previousDeltaTime; }

    std::array<Uint8, 4> pixelAt(int x, int y) const {
        const SDL_Rect area{x, y, 1, 1};
        std::array<Uint8, 4> pixel{};
        REQUIRE(SDL_RenderReadPixels(App::renderer, &area, SDL_PIXELFORMAT_RGBA32, pixel.data(), 4) == 0);
        return pixel;
    }

    void savePreview() const {
        SDL_RenderPresent(App::renderer);
        REQUIRE(SDL_SaveBMP(surface.get(), "player-scene-preview.bmp") == 0);
    }

    int drawCharacter(ecs::Registry& world, const CameraComponent* camera = nullptr) {
        SDL_SetRenderDrawColor(App::renderer, 0, 0, 0, 255);
        REQUIRE(SDL_RenderClear(App::renderer) == 0);
        renderSprites(world, App::renderer, camera);
        std::array<Uint32, 256 * 256> pixels{};
        REQUIRE(SDL_RenderReadPixels(App::renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                    pixels.data(), 256 * 4) == 0);
        int visiblePixels = 0;
        for (const auto pixel : pixels) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);
            if (r || g || b) ++visiblePixels;
        }
        return visiblePixels;
    }
};

TEST_CASE_METHOD(PlayerSDLFixture, "Player factory loads original sheets without choosing a controller",
                 "[integration][player][animation]") {
    PlayerCanvas canvas;
    ecs::Registry world;
    const auto player = spawnPlayer(world, {128, 150});
    REQUIRE(player);
    REQUIRE(world.size() == 1);
    REQUIRE(world.hasComponent<PlayerComponent>(player));
    REQUIRE_FALSE(world.hasComponent<KeyboardControlComponent>(player));
    REQUIRE_FALSE(world.hasComponent<CameraComponent>(player));
    const auto& clips = world.getComponent<DirectionalAnimationComponent>(player);
    int width = 0, height = 0;
    REQUIRE(SDL_QueryTexture(clips.idle.texture.get(), nullptr, nullptr, &width, &height) == 0);
    REQUIRE(width == 768);
    REQUIRE(height == 512);
    REQUIRE(SDL_QueryTexture(clips.walk.texture.get(), nullptr, nullptr, &width, &height) == 0);
    REQUIRE(width == 512);
    REQUIRE(height == 512);
    SDL_ScaleMode scaleMode;
    REQUIRE(SDL_GetTextureScaleMode(clips.walk.texture.get(), &scaleMode) == 0);
    REQUIRE(scaleMode == SDL_ScaleModeNearest);
    REQUIRE(canvas.drawCharacter(world) > 100);
}

TEST_CASE_METHOD(PlayerSDLFixture, "Walk and idle select all eight supplied directions and complete clips",
                 "[integration][player][animation]") {
    PlayerCanvas canvas;
    ecs::Registry world;
    const auto player = spawnPlayer(world, {128, 150});
    REQUIRE(player);
    auto& input = world.getComponent<MovementInputComponent>(player);
    auto& animation = world.getComponent<AnimationComponent>(player);
    auto& sprite = world.getComponent<SpriteComponent>(player);
    const auto& clips = world.getComponent<DirectionalAnimationComponent>(player);
    const std::array<math::Vector2, 8> directions{{{0,-1}, {1,-1}, {1,0}, {1,1},
                                                 {0,1}, {-1,1}, {-1,0}, {-1,-1}}};
    const std::array<int, 8> rows{{3, 6, 5, 4, 7, 2, 1, 0}};
    for (std::size_t i = 0; i < directions.size(); ++i) {
        input.direction = directions[i];
        updateMovement(world, 0.01f);
        updateDirectionalAnimations(world);
        REQUIRE(sprite.texture == clips.walk.texture);
        REQUIRE(animation.row == rows[i]);
        REQUIRE(animation.frameCount == 8);
        updateAnimations(world, 0.7);
        // Visit every frame, including the final column, and check that it renders.
        for (int frame = 0; frame < 8; ++frame) {
            animation.currentFrame = frame;
            updateAnimations(world, 0.0);
            REQUIRE(sprite.source.x == frame * 64);
            REQUIRE(sprite.source.y == rows[i] * 64);
            REQUIRE(canvas.drawCharacter(world) > 100);
        }

        input.direction = {};
        updateMovement(world, 0.01f);
        updateDirectionalAnimations(world);
        REQUIRE(sprite.texture == clips.idle.texture);
        REQUIRE(animation.row == rows[i]);
        REQUIRE(animation.frameCount == 12);
        for (int frame = 0; frame < 12; ++frame) {
            animation.currentFrame = frame;
            updateAnimations(world, 0.0);
            REQUIRE(sprite.source.x == frame * 64);
            REQUIRE(canvas.drawCharacter(world) > 100);
        }
    }
}

TEST_CASE_METHOD(PlayerSDLFixture, "Continuous walking preserves animation time and release restarts idle once",
                 "[integration][player][animation]") {
    PlayerCanvas canvas;
    ecs::Registry world;
    const auto player = spawnPlayer(world, {128, 150});
    REQUIRE(player);
    auto& input = world.getComponent<MovementInputComponent>(player);
    auto& animation = world.getComponent<AnimationComponent>(player);
    input.direction = {1, 0};
    for (int step = 0; step < 2; ++step) {
        updateMovement(world, 0.125f);
        updateDirectionalAnimations(world);
        updateAnimations(world, 0.125);
    }
    REQUIRE(animation.currentFrame == 2);
    REQUIRE(std::abs(animation.elapsed - 0.05) < 0.00001);

    input.direction = {};
    updateMovement(world, 0.125f);
    updateDirectionalAnimations(world);
    REQUIRE(animation.currentFrame == 0);
    REQUIRE(animation.elapsed == 0.0);
    REQUIRE(animation.row == 5); // Idle still faces east.
    updateAnimations(world, 0.25);
    updateDirectionalAnimations(world);
    REQUIRE(animation.currentFrame == 2);
    updateAnimations(world, 1.25);
    REQUIRE(animation.currentFrame == 0); // Full 12-frame idle loop.
}

class ControlledPlayerScene : public GameScene {
public:
    ecs::Entity player;
    ControlledPlayerScene() {
        player = spawnPlayer(world, {100, 200});
        REQUIRE(player);
        cameraEntity = player;
        auto& view = world.addComponent<CameraComponent>(player);
        view.size = {128, 128};
        view.target = player;
        view.following = true;
    }
    void move(math::Vector2 direction) { world.getComponent<MovementInputComponent>(player).direction = direction; }
    math::Vector2 position() { return world.getComponent<TransformComponent>(player).position; }
    CameraComponent& view() { return *camera(); }
    AnimationComponent& animation() { return world.getComponent<AnimationComponent>(player); }
    std::size_t entityCount() { return world.size(); }
};

TEST_CASE_METHOD(PlayerSDLFixture, "Scene systems move externally controlled player and its attached camera",
                 "[integration][player][camera]") {
    PlayerCanvas canvas;
    ControlledPlayerScene scene;
    scene.move({1, 0}); // A future network controller writes the same intent.
    App::deltaTime = 0.125f;
    scene.update(canvas.app);
    REQUIRE(scene.entityCount() == 1);
    REQUIRE(scene.position().x == 120.0f);
    REQUIRE(scene.position().y == 200.0f);
    REQUIRE(scene.view().target == scene.player);
    REQUIRE(scene.view().position.x == 56.0f);
    REQUIRE(scene.view().position.y == 136.0f);
    REQUIRE(scene.animation().row == 5);
    REQUIRE(scene.animation().frameCount == 8);
    REQUIRE(scene.animation().currentFrame == 1);
    scene.render(canvas.app);
}

TEST_CASE_METHOD(PlayerSDLFixture, "Game scene renders supplied player at the followed map position",
                 "[integration][player][camera]") {
    PlayerCanvas canvas(1280, 720);
    std::unique_ptr<Scene> scene(createScene2());
    scene->onEnter(canvas.app);
    App::deltaTime = 0.0f;
    scene->update(canvas.app);
    scene->render(canvas.app);
    // Cyan head pixel from frame 0, south-facing row 7, at the camera's center.
    const std::array<Uint8, 4> head{{95, 205, 228, 255}};
    REQUIRE(canvas.pixelAt(638, 314) == head);
    canvas.savePreview();
    scene->onExit(canvas.app);
}
}
