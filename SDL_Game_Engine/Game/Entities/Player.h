#pragma once

#include "../../Engine/ECS/ECS.h"
#include "../../Engine/Math/Vector2.h"

struct PlayerComponent {};

inline constexpr math::Vector2 playerHitboxHalfSize{10.0f, 5.0f};

ecs::Entity spawnPlayer(ecs::Registry& world, math::Vector2 position);
