#pragma once

#include "../ECS.h"
#include "../../Math/Vector2.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>

struct CameraComponent {
    math::Vector2 position{};
    math::Vector2 size{1280.0f, 720.0f};
    float speed = 500.0f;
    ecs::Entity target{};
    bool following = false;
    bool bounded = false;
    SDL_FRect bounds{};

    bool isValid() const noexcept {
        return size.x > 0.0f && size.y > 0.0f && std::isfinite(size.x) && std::isfinite(size.y) &&
               std::isfinite(position.x) && std::isfinite(position.y) &&
               std::isfinite(position.x + size.x) && std::isfinite(position.y + size.y);
    }

    bool isVisible(const SDL_FRect& rectangle) const noexcept {
        return isValid() && rectangle.w > 0.0f && rectangle.h > 0.0f &&
               std::isfinite(rectangle.x) && std::isfinite(rectangle.y) &&
               std::isfinite(rectangle.w) && std::isfinite(rectangle.h) &&
               static_cast<double>(rectangle.x) < static_cast<double>(position.x) + size.x &&
               static_cast<double>(rectangle.y) < static_cast<double>(position.y) + size.y &&
               static_cast<double>(rectangle.x) + rectangle.w > position.x &&
               static_cast<double>(rectangle.y) + rectangle.h > position.y;
    }

    void clampToBounds() noexcept {
        if (!bounded || !isValid() || bounds.w <= 0.0f || bounds.h <= 0.0f ||
            !std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
            !std::isfinite(bounds.w) || !std::isfinite(bounds.h) ||
            !std::isfinite(bounds.x + bounds.w) || !std::isfinite(bounds.y + bounds.h)) return;
        position.x = bounds.w <= size.x ? bounds.x + (bounds.w - size.x) * 0.5f :
            std::clamp(position.x, bounds.x, bounds.x + bounds.w - size.x);
        position.y = bounds.h <= size.y ? bounds.y + (bounds.h - size.y) * 0.5f :
            std::clamp(position.y, bounds.y, bounds.y + bounds.h - size.y);
    }

    void move(math::Vector2 direction, float deltaTime) noexcept {
        if (!isValid() || deltaTime <= 0.0f || !std::isfinite(deltaTime) ||
            speed <= 0.0f || !std::isfinite(speed) || !std::isfinite(direction.x) ||
            !std::isfinite(direction.y) || (direction.x == 0.0f && direction.y == 0.0f)) return;
        const auto next = position + direction.normalized() * speed * deltaTime;
        if (!std::isfinite(next.x) || !std::isfinite(next.y) ||
            !std::isfinite(next.x + size.x) || !std::isfinite(next.y + size.y)) return;
        position = next;
        following = false;
        clampToBounds();
    }
};
