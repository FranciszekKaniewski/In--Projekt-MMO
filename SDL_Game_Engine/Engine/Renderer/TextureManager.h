#pragma once
#include "../Core/Font.h"

class TextureManager{
public:
    static SDL_Texture* LoadTexture(const char* fileName);
    static SDL_Texture* LoadTextTexture(Font font, const std::string& label);

    static void DrawRectangle(SDL_Rect rect, SDL_Color color, int cornerRadius = 0);
};
