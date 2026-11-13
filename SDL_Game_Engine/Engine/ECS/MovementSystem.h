#pragma once

#include "../Math/Vector2.h"

namespace ecs { class Registry; }
enum class FacingDirection;

FacingDirection movementFacing(math::Vector2 direction);
void updateMovement(ecs::Registry& world, float deltaTime);
