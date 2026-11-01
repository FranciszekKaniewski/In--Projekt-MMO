#include "MovementSystem.h"

#include "ECS.h"
#include "Components/MovementBoundsComponent.h"
#include "Components/MovementComponent.h"
#include "Components/MovementInputComponent.h"
#include "Components/TransformComponent.h"
#include "Components/VelocityComponent.h"

#include <algorithm>
#include <cmath>

namespace {
FacingDirection facingFor(math::Vector2 direction) {
    if (direction.y < 0.0f) {
        if (direction.x < 0.0f) return FacingDirection::NorthWest;
        if (direction.x > 0.0f) return FacingDirection::NorthEast;
        return FacingDirection::North;
    }
    if (direction.y > 0.0f) {
        if (direction.x < 0.0f) return FacingDirection::SouthWest;
        if (direction.x > 0.0f) return FacingDirection::SouthEast;
        return FacingDirection::South;
    }
    return direction.x < 0.0f ? FacingDirection::West : FacingDirection::East;
}
}

void updateMovement(ecs::Registry& world, float deltaTime) {
    if (deltaTime < 0.0f || !std::isfinite(deltaTime)) return;

    world.forEach<MovementInputComponent, MovementComponent, VelocityComponent>(
        [](ecs::Entity, const MovementInputComponent& input, MovementComponent& movement,
           VelocityComponent& velocity) {
            auto direction = input.direction;
            if (!std::isfinite(direction.x) || !std::isfinite(direction.y)) direction = {};
            if (direction.lengthSquared() > 1.0f) direction.normalize();
            if (direction.x != 0.0f || direction.y != 0.0f) movement.facing = facingFor(direction);

            const float speed = std::isfinite(movement.speed) && movement.speed > 0.0f
                ? movement.speed : 0.0f;
            velocity.vectors["movement"] = direction * speed;
        });

    world.forEach<TransformComponent, VelocityComponent>(
        [&world, deltaTime](ecs::Entity entity, TransformComponent& transform,
                           const VelocityComponent& velocity) {
            auto next = transform.position + velocity.total() * deltaTime;
            auto* movement = world.tryGetComponent<MovementComponent>(entity);
            if (movement) movement->moving = false;
            if (!std::isfinite(next.x) || !std::isfinite(next.y)) return;

            const auto* bounds = world.tryGetComponent<MovementBoundsComponent>(entity);
            if (bounds && std::isfinite(bounds->minimum.x) && std::isfinite(bounds->minimum.y) &&
                std::isfinite(bounds->maximum.x) && std::isfinite(bounds->maximum.y) &&
                bounds->minimum.x <= bounds->maximum.x && bounds->minimum.y <= bounds->maximum.y) {
                next.x = std::clamp(next.x, bounds->minimum.x, bounds->maximum.x);
                next.y = std::clamp(next.y, bounds->minimum.y, bounds->maximum.y);
            }

            if (movement) {
                movement->moving = next.x != transform.position.x || next.y != transform.position.y;
            }
            transform.position = next;
        });
}