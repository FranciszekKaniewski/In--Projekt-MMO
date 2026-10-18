#include "SpriteRenderer.h"

#include "../ECS/ECS.h"
#include "../ECS/Components/SpriteComponent.h"
#include "../ECS/Components/TransformComponent.h"

#include <cmath>

void renderSprites(ecs::Registry& world, SDL_Renderer* renderer) {
    if (!renderer) return;

    world.forEach<TransformComponent, SpriteComponent>(
        [renderer](ecs::Entity, const TransformComponent& transform, const SpriteComponent& sprite) {
            if (!sprite.visible || !sprite.texture || sprite.source.x < 0 || sprite.source.y < 0 ||
                sprite.source.w <= 0 || sprite.source.h <= 0 ||
                sprite.size.x <= 0.0f || sprite.size.y <= 0.0f ||
                !std::isfinite(sprite.size.x) || !std::isfinite(sprite.size.y)) return;

            int textureWidth = 0, textureHeight = 0;
            if (SDL_QueryTexture(sprite.texture.get(), nullptr, nullptr, &textureWidth, &textureHeight) != 0 ||
                sprite.source.w > textureWidth || sprite.source.h > textureHeight ||
                sprite.source.x > textureWidth - sprite.source.w ||
                sprite.source.y > textureHeight - sprite.source.h) return;

            const auto center = transform.position + sprite.offset;
            const SDL_FRect destination{center.x - sprite.size.x * 0.5f,
                                        center.y - sprite.size.y * 0.5f,
                                        sprite.size.x, sprite.size.y};
            if (!std::isfinite(destination.x) || !std::isfinite(destination.y) ||
                !std::isfinite(destination.x + destination.w) ||
                !std::isfinite(destination.y + destination.h)) return;

            SDL_RenderCopyExF(renderer, sprite.texture.get(), &sprite.source, &destination,
                              0.0, nullptr, sprite.flip);
        });
}
