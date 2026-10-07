#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include "./UIElement.h"
#include "../Core/App.h"
#include "../Renderer/TextureManager.h"

class Slider : public UIElement {
private:
    int minimum;
    int maximum;
    int value;
    int step;
    bool hovered = false;
    bool focused = false;
    bool dragging = false;

    int thumbRadius() const {
        return std::max(1, std::min({12, rect.h / 2, rect.w / 2}));
    }

    void updateFromMouse(int x) {
        const int radius = thumbRadius();
        const int width = rect.w - 2 * radius;
        if(width <= 0) return;

        const double ratio = std::clamp(
            (static_cast<double>(x) - rect.x - radius) / width, 0.0, 1.0);
        setValue(static_cast<int>(std::lround(
            minimum + ratio * (static_cast<double>(maximum) - minimum))));
    }

public:
    SDL_Color trackColor = {222,226,235,255};
    SDL_Color accentColor = {60,120,220,255};
    SDL_Color thumbColor = {255,255,255,255};
    std::function<void(int)> onChange;

    Slider(int minimum = 0, int maximum = 100, int value = 50,
           SDL_Rect rect = {0,0,280,40}, int step = 1,
           std::function<void(int)> onChange = {})
        : UIElement(rect), minimum(std::min(minimum, maximum)),
          maximum(std::max(minimum, maximum)), value(this->minimum),
          step(std::max(1, step)) {
        setValue(value);
        this->onChange = std::move(onChange);
    }

    int getValue() const { return value; }
    int getMinimum() const { return minimum; }
    int getMaximum() const { return maximum; }

    void setValue(int newValue) {
        const int clamped = std::clamp(newValue, minimum, maximum);
        const double snapped = minimum + std::round(
            (static_cast<double>(clamped) - minimum) / step) * step;
        const int next = clamped == maximum ? maximum : static_cast<int>(
            std::clamp(snapped, static_cast<double>(minimum),
                       static_cast<double>(maximum)));
        if(next == value) return;
        value = next;
        if(onChange) onChange(value);
    }

    void setHover(bool hover) override { hovered = hover; }
    bool isFocusable() const override { return true; }
    bool isFocused() const override { return focused; }
    bool hasMouseCapture() const override { return dragging; }

    void setFocused(bool focus) override {
        focused = focus;
        if(!focus) dragging = false;
    }

    bool handleEvent(const SDL_Event& event) override {
        if(event.type == SDL_MOUSEBUTTONDOWN &&
           event.button.button == SDL_BUTTON_LEFT &&
           containsPoint(event.button.x, event.button.y)) {
            dragging = true;
            updateFromMouse(event.button.x);
            return true;
        }
        if(event.type == SDL_MOUSEMOTION && dragging) {
            updateFromMouse(event.motion.x);
            return true;
        }
        if(event.type == SDL_MOUSEBUTTONUP &&
           event.button.button == SDL_BUTTON_LEFT && dragging) {
            dragging = false;
            updateFromMouse(event.button.x);
            return true;
        }
        if(!focused) return false;
        if(event.type == SDL_KEYUP) return true;
        if(event.type != SDL_KEYDOWN) return false;

        switch(event.key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_DOWN:
                setValue(static_cast<int>(std::max(
                    static_cast<double>(minimum), static_cast<double>(value) - step)));
                return true;
            case SDLK_RIGHT:
            case SDLK_UP:
                setValue(static_cast<int>(std::min(
                    static_cast<double>(maximum), static_cast<double>(value) + step)));
                return true;
            case SDLK_HOME: setValue(minimum); return true;
            case SDLK_END: setValue(maximum); return true;
            case SDLK_ESCAPE: setFocused(false); return true;
            default: return false;
        }
    }

    void draw() override {
        if(!App::renderer || rect.w <= 2 || rect.h <= 2) return;
        const int radius = thumbRadius();
        const int width = rect.w - 2 * radius;
        const double ratio = maximum == minimum ? 0.0 :
            (static_cast<double>(value) - minimum) /
            (static_cast<double>(maximum) - minimum);
        const int filled = static_cast<int>(std::lround(ratio * width));
        const int centerY = rect.y + rect.h / 2;
        const int trackHeight = std::min(8, rect.h);
        SDL_Rect track = {rect.x + radius, centerY - trackHeight / 2,
                          width, trackHeight};
        TextureManager::DrawRectangle(track, trackColor, trackHeight / 2);
        track.w = filled;
        TextureManager::DrawRectangle(track, accentColor, trackHeight / 2);

        SDL_Rect thumb = {rect.x + filled, centerY - radius, radius * 2, radius * 2};
        const SDL_Color outline = focused || hovered ? accentColor : trackColor;
        TextureManager::DrawRectangle(thumb, outline, radius);
        thumb = {thumb.x + 3, thumb.y + 3, thumb.w - 6, thumb.h - 6};
        TextureManager::DrawRectangle(thumb, thumbColor, std::max(0, radius - 3));
    }

    void clean() override { setFocused(false); }
};
