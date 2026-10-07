#include "../Core/App.h"
#include "./TextureManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

#include "../Vendor/SDL2/include/SDL2/SDL_image.h"

SDL_Texture* TextureManager::LoadTexture(const char *fileName) {
    if (!fileName || !*fileName || std::strcmp(fileName, "none") == 0) return nullptr;
    if (!App::renderer) {
        std::cerr << "Cannot load texture without a renderer: " << fileName << std::endl;
        return nullptr;
    }

    SDL_Surface* tempSurface = IMG_Load(fileName);
    if (!tempSurface) {
        std::cerr << "Failed to load image '" << fileName << "': " << IMG_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(App::renderer,tempSurface);
    SDL_FreeSurface(tempSurface);

    if (!tex) {
        std::cerr << "Failed to create image texture '" << fileName << "': "
                  << SDL_GetError() << std::endl;
        return nullptr;
    }
    if (SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND) != 0 ||
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear) != 0) {
        std::cerr << "Failed to configure image texture '" << fileName << "': "
                  << SDL_GetError() << std::endl;
        SDL_DestroyTexture(tex);
        return nullptr;
    }

    return tex;
}

SDL_Texture* TextureManager::LoadTextTexture(Font font, const std::string& label) {
    if (label.empty()) return nullptr;
    if (!font.font || !App::renderer) {
        std::cerr << "Cannot render text without a valid font and renderer." << std::endl;
        return nullptr;
    }

    SDL_Surface* tempSurface = TTF_RenderUTF8_Blended(font.font, label.c_str(), font.color);
    if (!tempSurface) {
        std::cerr << "Failed to render text: " << TTF_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(App::renderer,tempSurface);
    SDL_FreeSurface(tempSurface);

    if (!tex) {
        std::cerr << "Failed to create text texture: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);

    return tex;
}

void TextureManager::DrawRectangle(SDL_Rect rect, SDL_Color color, int cornerRadius){
    if (rect.w <= 0 || rect.h <= 0) return;

    int radius = std::clamp(cornerRadius, 0, std::min(rect.w, rect.h) / 2);
    Uint8 prevR, prevG, prevB, prevA;

    SDL_GetRenderDrawColor(App::renderer, &prevR, &prevG, &prevB, &prevA);
    SDL_SetRenderDrawColor(App::renderer, color.r, color.g, color.b, color.a);
    if (radius == 0) {
        SDL_RenderFillRect(App::renderer, &rect);
    } else {
        SDL_Rect middle = {rect.x, rect.y + radius, rect.w, rect.h - 2 * radius};
        if (middle.h > 0) SDL_RenderFillRect(App::renderer, &middle);

        for (int row = 0; row < radius; ++row) {
            // Sample pixel centers for symmetric corners. Each row is drawn once,
            // so translucent rectangles do not have darker overlapping sections.
            double dy = radius - row - 0.5;
            double dx = std::sqrt(static_cast<double>(radius) * radius - dy * dy);
            int inset = static_cast<int>(std::ceil(radius - dx - 0.5));
            int left = rect.x + inset;
            int right = rect.x + rect.w - inset - 1;

            SDL_RenderDrawLine(App::renderer, left, rect.y + row, right, rect.y + row);
            SDL_RenderDrawLine(App::renderer, left, rect.y + rect.h - row - 1,
                               right, rect.y + rect.h - row - 1);
        }
    }
    SDL_SetRenderDrawColor(App::renderer, prevR, prevG, prevB, prevA);
}
