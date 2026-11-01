#include "Player.h"

#include "../../Engine/ECS/Components/AnimationComponent.h"
#include "../../Engine/ECS/Components/ColliderComponent.h"
#include "../../Engine/ECS/Components/DirectionalAnimationComponent.h"
#include "../../Engine/ECS/Components/MovementComponent.h"
#include "../../Engine/ECS/Components/MovementInputComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/ECS/Components/VelocityComponent.h"
#include "../../Engine/Renderer/TextureManager.h"

#include <iostream>

namespace {
TexturePtr loadSheet(const char* path, int frameCount) {
    auto texture = TextureManager::LoadSharedTexture(path);
    if (!texture) return {};
    int width = 0, height = 0;
    if (SDL_QueryTexture(texture.get(), nullptr, nullptr, &width, &height) != 0 ||
        width != frameCount * 64 || height != 8 * 64 ||
        SDL_SetTextureScaleMode(texture.get(), SDL_ScaleModeNearest) != 0) {
        std::cerr << "Invalid player sprite sheet (expected " << frameCount * 64
                  << " x 512): " << path << '\n';
        return {};
    }
    return texture;
}
}

ecs::Entity spawnPlayer(ecs::Registry& world, math::Vector2 position) {
    auto idle = loadSheet("Assets/Player/player_idle_full_sprite_sheet.png", 12);
    auto walk = loadSheet("Assets/Player/player_walk_complete_sprite_sheet.png", 8);
    if (!idle || !walk) return {};

    const auto player = world.createEntity();
    world.addComponent<PlayerComponent>(player);
    world.addComponent<TransformComponent>(player, position);
    world.addComponent<VelocityComponent>(player);
    world.addComponent<MovementInputComponent>(player);
    world.addComponent<MovementComponent>(player);
    world.addComponent<ColliderComponent>(player, ColliderComponent::box({20.0f, 10.0f}));

    auto& clips = world.addComponent<DirectionalAnimationComponent>(player);
    clips.idle = {idle, 64, 64, 12, 0.125};
    clips.walk = {walk, 64, 64, 8, 0.1};
    clips.rows = {{3, 6, 5, 4, 7, 2, 1, 0}};

    auto& sprite = world.addComponent<SpriteComponent>(player);
    sprite.texture = idle;
    sprite.source = {0, clips.rows[static_cast<std::size_t>(FacingDirection::South)] * 64, 64, 64};
    sprite.size = {128.0f, 128.0f};
    sprite.offset = {0.0f, -24.0f};

    auto& animation = world.addComponent<AnimationComponent>(player);
    animation.row = sprite.source.y / 64;
    animation.frameCount = clips.idle.frameCount;
    animation.frameDuration = clips.idle.frameDuration;
    return player;
}