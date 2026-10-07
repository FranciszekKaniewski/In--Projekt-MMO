#pragma once

#include <cstring>
#include <memory>
#include "../Renderer/TextureManager.h"
#include "../Core/App.h"

class UIBackground {
private:
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> image{nullptr, SDL_DestroyTexture};
    Uint8 opacity = 255;

public:
    bool setImage(const char* fileName) {
        if (!fileName || !*fileName || std::strcmp(fileName, "none") == 0) {
            clearImage();
            return true;
        }

        SDL_Texture* replacement = TextureManager::LoadTexture(fileName);
        if (!replacement) return false;
        image.reset(replacement);
        return true;
    }

    void clearImage() {
        image.reset();
    }

    void setOpacity(Uint8 alpha) {
        opacity = alpha;
    }

    void draw(SDL_Rect bounds, SDL_Color color, int cornerRadius, Uint8 modulation = 255) const {
        if (!App::renderer || bounds.w <= 0 || bounds.h <= 0) return;

        if (color.a != 0) {
            SDL_Color drawColor = {
                static_cast<Uint8>(color.r * modulation / 255),
                static_cast<Uint8>(color.g * modulation / 255),
                static_cast<Uint8>(color.b * modulation / 255), color.a
            };
            TextureManager::DrawRectangle(bounds, drawColor, cornerRadius);
        }
        if (!image || opacity == 0) return;

        Uint8 previousR, previousG, previousB, previousAlpha;
        SDL_GetTextureColorMod(image.get(), &previousR, &previousG, &previousB);
        SDL_GetTextureAlphaMod(image.get(), &previousAlpha);
        SDL_SetTextureColorMod(image.get(),
            static_cast<Uint8>(previousR * modulation / 255),
            static_cast<Uint8>(previousG * modulation / 255),
            static_cast<Uint8>(previousB * modulation / 255));
        SDL_SetTextureAlphaMod(image.get(), static_cast<Uint8>(previousAlpha * opacity / 255));
        SDL_RenderCopy(App::renderer, image.get(), nullptr, &bounds);
        SDL_SetTextureColorMod(image.get(), previousR, previousG, previousB);
        SDL_SetTextureAlphaMod(image.get(), previousAlpha);
    }
};
