#pragma once

enum class FacingDirection {
    North, NorthEast, East, SouthEast, South, SouthWest, West, NorthWest
};

struct MovementComponent {
    float speed = 160.0f; // World units per second.
    FacingDirection facing = FacingDirection::South;
    bool moving = false;
};
