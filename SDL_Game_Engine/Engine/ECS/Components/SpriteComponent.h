#pragma once

#include "../../Math/Vector2.h"

#include <SDL.h>
#include <memory>

using TexturePtr = std::shared_ptr<SDL_Texture>;

struct SpriteComponent {
    TexturePtr texture;
    SDL_Rect source{0, 0, 64, 64}; // Pixels in the source image.
    math::Vector2 size{64.0f, 64.0f}; // Display size in world units.
    math::Vector2 offset{}; // Relative to TransformComponent's center.
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    bool visible = true;
};
