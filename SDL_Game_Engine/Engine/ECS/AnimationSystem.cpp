#include "AnimationSystem.h"

#include "ECS.h"
#include "Components/AnimationComponent.h"
#include "Components/SpriteComponent.h"

#include <cmath>
#include <cstdint>
#include <limits>

void updateAnimations(ecs::Registry& world, double deltaTime) {
    if (deltaTime < 0.0 || !std::isfinite(deltaTime)) return;

    world.forEach<SpriteComponent, AnimationComponent>(
        [deltaTime](ecs::Entity, SpriteComponent& sprite, AnimationComponent& animation) {
            if (!animation.isValid() || sprite.source.w <= 0 || sprite.source.h <= 0) return;

            // Work on a copy so invalid coordinates never partially advance the animation.
            auto next = animation;
            if (next.playing) {
                const double elapsed = next.elapsed + deltaTime;
                const double steps = std::floor(elapsed / next.frameDuration);
                if (!std::isfinite(steps)) return;

                // Use the same step count for the remainder. fmod(elapsed, duration)
                // can disagree with floor(elapsed / duration) at frame boundaries.
                next.elapsed = elapsed - steps * next.frameDuration;
                if (!std::isfinite(next.elapsed)) return;
                next.elapsed = next.elapsed > 0.0 ? std::fmod(next.elapsed, next.frameDuration) : 0.0;
                if (!next.loop && steps >= next.frameCount - next.currentFrame) {
                    next.currentFrame = next.frameCount - 1;
                    next.elapsed = 0.0;
                    next.playing = false;
                } else {
                    // Constant time even after a long update or with very short frames.
                    const int advance = static_cast<int>(std::fmod(steps, next.frameCount));
                    next.currentFrame = static_cast<int>(
                        (static_cast<std::int64_t>(next.currentFrame) + advance) % next.frameCount);
                }
            }

            const auto x = static_cast<std::int64_t>(next.currentFrame) * sprite.source.w;
            const auto y = static_cast<std::int64_t>(next.row) * sprite.source.h;
            if (x > std::numeric_limits<int>::max() || y > std::numeric_limits<int>::max()) return;

            animation = next;
            sprite.source.x = static_cast<int>(x);
            sprite.source.y = static_cast<int>(y);
        });
}
