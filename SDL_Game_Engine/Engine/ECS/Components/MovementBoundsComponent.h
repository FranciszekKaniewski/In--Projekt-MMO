#pragma once

#include "../../Math/Vector2.h"

// Movement limits
struct MovementBoundsComponent {
    math::Vector2 minimum{};
    math::Vector2 maximum{};
};
