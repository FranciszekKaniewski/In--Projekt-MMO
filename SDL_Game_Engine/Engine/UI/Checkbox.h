#pragma once

#include <algorithm>
#include <functional>
#include <utility>
#include "./UIElement.h"
#include "../Core/App.h"
#include "../Renderer/TextureManager.h"

class Checkbox : public UIElement {
private:
    bool checked;
    bool hovered = false;
    bool focused = false;

public:
    SDL_Color bgColor = {255,255,255,255};
    SDL_Color borderColor = {170,170,170,255};
    SDL_Color accentColor = {60,120,220,255};
    SDL_Color checkColor = {255,255,255,255};
    int cornerRadius = 6;
    std::function<void(bool)> onChange;

    Checkbox(bool checked = false, SDL_Rect rect = {0,0,32,32},
             std::function<void(bool)> onChange = {})
        : UIElement(rect), checked(checked), onChange(std::move(onChange)) {}

    bool isChecked() const { return checked; }

    void setChecked(bool value) {
        if(checked == value) return;
        checked = value;
        if(onChange) onChange(checked);
    }

    void setHover(bool hover) override { hovered = hover; }
    bool isFocusable() const override { return true; }
    bool isFocused() const override { return focused; }
    void setFocused(bool focus) override { focused = focus; }

    void handleClick() override {
        if(hovered) setChecked(!checked);
    }

    bool handleEvent(const SDL_Event& event) override {
        if(!focused) return false;
        if(event.type == SDL_KEYUP) return true;
        if(event.type != SDL_KEYDOWN) return false;
        switch(event.key.keysym.sym) {
            case SDLK_SPACE:
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if(!event.key.repeat) setChecked(!checked);
                return true;
            case SDLK_ESCAPE: setFocused(false); return true;
            default: return false;
        }
    }

    void draw() override {
        if(!App::renderer || rect.w <= 4 || rect.h <= 4) return;
        TextureManager::DrawRectangle(rect,
            checked || focused || hovered ? accentColor : borderColor, cornerRadius);
        SDL_Rect inner = {rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4};
        TextureManager::DrawRectangle(inner, checked ? accentColor : bgColor,
                                      std::max(0, cornerRadius - 2));
        if(!checked) return;

        Uint8 r, g, b, a;
        SDL_GetRenderDrawColor(App::renderer, &r, &g, &b, &a);
        SDL_SetRenderDrawColor(App::renderer, checkColor.r, checkColor.g,
                               checkColor.b, checkColor.a);
        const SDL_Point first = {rect.x + rect.w / 4, rect.y + rect.h / 2};
        const SDL_Point middle = {rect.x + rect.w * 2 / 5, rect.y + rect.h * 2 / 3};
        const SDL_Point last = {rect.x + rect.w * 3 / 4, rect.y + rect.h / 3};
        for(int offset = -1; offset <= 1; ++offset) {
            SDL_RenderDrawLine(App::renderer, first.x, first.y + offset,
                               middle.x, middle.y + offset);
            SDL_RenderDrawLine(App::renderer, middle.x, middle.y + offset,
                               last.x, last.y + offset);
        }
        SDL_SetRenderDrawColor(App::renderer, r, g, b, a);
    }

    void clean() override { setFocused(false); }
};
