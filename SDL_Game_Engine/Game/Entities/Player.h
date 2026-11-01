#pragma once

#include "../../Engine/ECS/ECS.h"
#include "../../Engine/Math/Vector2.h"

struct PlayerComponent {};

ecs::Entity spawnPlayer(ecs::Registry& world, math::Vector2 position);