#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include "./Label.h"

class Button : public Label {
private:
    bool isHovered = false;

public:
    float hoverScale;
    std::function<void()> onClick;

    Button(Font font, const std::string& text,
           SDL_Color bgColor = {60, 120, 220, 255},
           SDL_Rect rect = {0, 0, 128, 64},
           float hoverScale = 1.05f, std::function<void()> onClick = {}) :
        Label(font, text, bgColor, rect),
        hoverScale(hoverScale), onClick(std::move(onClick)) {}

    ~Button() override {
        clean();
    }

    void setHover(bool hover) override {
        isHovered = hover;
    }

    void handleClick() override {
        if (!isHovered || !onClick) return;

        onClick();
    }

    void draw() override {
        if (rect.w <= 0 || rect.h <= 0) return;

        const float scale = isHovered ? std::max(1.0f, hoverScale) : 1.0f;
        SDL_Rect drawRect = rect;
        drawRect.w = static_cast<int>(std::lround(rect.w * scale));
        drawRect.h = static_cast<int>(std::lround(rect.h * scale));
        drawRect.x -= (drawRect.w - rect.w) / 2;
        drawRect.y -= (drawRect.h - rect.h) / 2;

        const Uint8 backgroundMod = isHovered ? 128 : 255;
        SDL_Color drawColor = {
            static_cast<Uint8>(bgColor.r * backgroundMod / 255),
            static_cast<Uint8>(bgColor.g * backgroundMod / 255),
            static_cast<Uint8>(bgColor.b * backgroundMod / 255),
            bgColor.a
        };
        TextureManager::DrawRectangle(drawRect, drawColor, bgCornerRadius);

        if (!texture) return;

        SDL_SetTextureColorMod(texture, 255, 255, 255);

        int textWidth, textHeight;
        if (SDL_QueryTexture(texture, nullptr, nullptr, &textWidth, &textHeight) != 0 ||
            textWidth <= 0 || textHeight <= 0) return;

        const int padding = static_cast<int>(std::lround(std::max(0, margin) * scale));
        const int availableWidth = drawRect.w - padding;
        const int availableHeight = drawRect.h - padding;
        if (availableWidth <= 0 || availableHeight <= 0) return;

        const float textScale = std::min(static_cast<float>(availableWidth) / textWidth,
                                         static_cast<float>(availableHeight) / textHeight);
        SDL_Rect textRect = {0, 0,
            std::max(1, static_cast<int>(std::lround(textWidth * textScale))),
            std::max(1, static_cast<int>(std::lround(textHeight * textScale)))};
        textRect.x = drawRect.x + (drawRect.w - textRect.w) / 2;
        textRect.y = drawRect.y + (drawRect.h - textRect.h) / 2;
        SDL_RenderCopy(App::renderer, texture, nullptr, &textRect);
    }
};
