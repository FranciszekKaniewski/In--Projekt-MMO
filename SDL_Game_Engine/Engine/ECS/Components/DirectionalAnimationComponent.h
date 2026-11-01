#pragma once

#include "SpriteComponent.h"

#include <array>

struct DirectionalAnimationClip {
    TexturePtr texture;
    int frameWidth = 64;
    int frameHeight = 64;
    int frameCount = 1;
    double frameDuration = 0.1;
};

struct DirectionalAnimationComponent {
    DirectionalAnimationClip idle;
    DirectionalAnimationClip walk;

    std::array<int, 8> rows{{0, 1, 2, 3, 4, 5, 6, 7}};
};
