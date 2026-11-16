#pragma once

#include <SDL.h>
#include <vector>

struct SpriteDrawCapture {
    struct Draw {
        SDL_Texture* texture;
        SDL_Rect source;
        SDL_FRect destination;
    };

    static inline SpriteDrawCapture* active = nullptr;
    std::vector<Draw> draws;

    SpriteDrawCapture() { active = this; }
    ~SpriteDrawCapture() { active = nullptr; }
    SpriteDrawCapture(const SpriteDrawCapture&) = delete;
    SpriteDrawCapture& operator=(const SpriteDrawCapture&) = delete;
};
