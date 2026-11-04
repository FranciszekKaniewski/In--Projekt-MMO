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
    bool enabled = true;

public:
    float hoverScale;
    std::function<void()> onClick;

    Button(Font font, const std::string& text,
           SDL_Color bgColor = {60, 120, 220, 255},
           SDL_Rect rect = {0, 0, 128, 64},
           float hoverScale = 1.05f, std::function<void()> onClick = {},
           const std::string& backgroundImage = "") :
        Label(font, text, bgColor, rect, backgroundImage),
        hoverScale(hoverScale), onClick(std::move(onClick)) {}

    ~Button() override {
        clean();
    }

    void setHover(bool hover) override {
        isHovered = enabled && hover;
    }

    void setEnabled(bool value) { enabled = value; if(!enabled) isHovered = false; }
    bool isEnabled() const { return enabled; }

    bool isInteractive() const override {
        return enabled;
    }

    void handleClick() override {
        if (!enabled || !isHovered || !onClick) return;

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

        const Uint8 backgroundMod = !enabled ? 100 : isHovered ? 128 : 255;
        drawBackground(drawRect, backgroundMod);

        if (!texture) return;

        const Uint8 textMod = enabled ? 255 : 140;
        SDL_SetTextureColorMod(texture, textMod, textMod, textMod);

        const int padding = static_cast<int>(std::lround(std::max(0, margin) * scale));
        SDL_Rect textBounds = {drawRect.x + padding / 2, drawRect.y + padding / 2,
                               drawRect.w - padding, drawRect.h - padding};
        drawText(textBounds, scale);
    }
};
