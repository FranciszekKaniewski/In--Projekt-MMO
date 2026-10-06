#pragma once
#include "../Vendor/SDL2/include/SDL2/SDL.h"

class UIElement {
private:
public:
    SDL_Rect rect;

    UIElement(SDL_Rect rect = {0, 0, 0, 0}) : rect(rect) {}
    virtual ~UIElement() = default;
    virtual void draw() {};
    virtual void setHover(bool hover) {};
    virtual void handleClick() {};
    virtual void clean() {};

    void changeRect(int x, int y, int w, int h) {
        this->rect = {x, y, w, h};
    }
};