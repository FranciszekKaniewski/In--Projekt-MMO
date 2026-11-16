#pragma once

#include "../../Math/Vector2.h"

class Map;

// Movement limits
struct MovementBoundsComponent {
    math::Vector2 minimum{};
    math::Vector2 maximum{};
    const Map* map = nullptr;
};
