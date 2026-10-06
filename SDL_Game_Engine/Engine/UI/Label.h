#pragma once
#include <algorithm>
#include <cmath>
#include "./UIElement.h"
#include "../Core/Font.h"
#include "../Renderer/TextureManager.h"
#include "../Core/App.h"

class Label : public UIElement {
public:
    std::string text;
    Font font;
    SDL_Color bgColor;
    SDL_Texture* texture = nullptr;
    int margin = 15;
    int bgCornerRadius = 15;

    Label(Font font,const std::string& text,SDL_Color bgColor={0,0,0,0}, SDL_Rect rect={0,0,0,0}) :
        UIElement(rect), text(text), font(font),bgColor(bgColor) {
        texture = TextureManager::LoadTextTexture(font,text);
    };

    void updateText(const std::string& newText) {
        text = newText;
        SDL_DestroyTexture(texture);
        texture = TextureManager::LoadTextTexture(font, text);
    }

    void draw() override {
        SDL_Rect bgRect = {rect.x-margin/2, rect.y-margin/2, rect.w + margin, rect.h + margin};
        TextureManager::DrawRectangle(bgRect,bgColor,bgCornerRadius);

        drawText(rect);
    }

    void clean() override {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }

protected:
    void drawText(SDL_Rect bounds, float maxScale = 1.0f) {
        if (!texture || bounds.w <= 0 || bounds.h <= 0) return;

        int textWidth, textHeight;
        if (SDL_QueryTexture(texture, nullptr, nullptr, &textWidth, &textHeight) != 0 ||
            textWidth <= 0 || textHeight <= 0) return;

        // Keep the font's natural size and proportions, shrinking only to fit.
        const float scale = std::min({maxScale,
            static_cast<float>(bounds.w) / textWidth,
            static_cast<float>(bounds.h) / textHeight});
        SDL_Rect textRect = {0, 0,
            std::max(1, static_cast<int>(std::lround(textWidth * scale))),
            std::max(1, static_cast<int>(std::lround(textHeight * scale)))};
        textRect.x = bounds.x + (bounds.w - textRect.w) / 2;
        textRect.y = bounds.y + (bounds.h - textRect.h) / 2;
        SDL_RenderCopy(App::renderer, texture, nullptr, &textRect);
    }
};
