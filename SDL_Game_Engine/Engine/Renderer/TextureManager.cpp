#include "../Core/App.h"
#include "./TextureManager.h"

#include <algorithm>
#include <cmath>

#include "../Vendor/SDL2/include/SDL2/SDL_image.h"

SDL_Texture* TextureManager::LoadTexture(const char *fileName) {
    if(strcmp(fileName, "none") == 0) return nullptr;

    SDL_Surface* tempSurface = IMG_Load(fileName);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(App::renderer,tempSurface);
    SDL_FreeSurface(tempSurface);

    return tex;
}

SDL_Texture* TextureManager::LoadTextTexture(Font font, const std::string& label) {

    SDL_Surface* tempSurface = TTF_RenderText_Solid(font.font, label.c_str(), font.color);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(App::renderer,tempSurface);
    SDL_FreeSurface(tempSurface);

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
