#include "DirectionalAnimationSystem.h"

#include "ECS.h"
#include "Components/AnimationComponent.h"
#include "Components/DirectionalAnimationComponent.h"
#include "Components/MovementComponent.h"

#include <cmath>

void updateDirectionalAnimations(ecs::Registry& world) {
    world.forEach<MovementComponent, DirectionalAnimationComponent, SpriteComponent, AnimationComponent>(
        [](ecs::Entity, const MovementComponent& movement, const DirectionalAnimationComponent& clips,
           SpriteComponent& sprite, AnimationComponent& animation) {
            const auto direction = static_cast<std::size_t>(movement.facing);
            const auto& clip = movement.moving ? clips.walk : clips.idle;
            if (direction >= clip.rows.size()) return;
            const int row = clip.rows[direction];
            if (!clip.texture || row < 0 || clip.frameWidth <= 0 || clip.frameHeight <= 0 ||
                clip.frameCount <= 0 || clip.frameDuration <= 0.0 ||
                !std::isfinite(clip.frameDuration)) return;

            if (sprite.texture == clip.texture && sprite.source.w == clip.frameWidth &&
                sprite.source.h == clip.frameHeight &&
                animation.frameCount == clip.frameCount && animation.frameDuration == clip.frameDuration) {
                animation.row = row;
                return; // Turning keeps the current frame and clock within the clip.
            }

            sprite.texture = clip.texture;
            sprite.source = {0, 0, clip.frameWidth, clip.frameHeight};
            animation.row = row;
            animation.frameCount = clip.frameCount;
            animation.frameDuration = clip.frameDuration;
            animation.loop = true;
            animation.restart();
        });
}
