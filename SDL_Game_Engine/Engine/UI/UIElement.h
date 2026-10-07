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
    virtual bool handleEvent(const SDL_Event& event) { return false; }
    virtual bool isFocusable() const { return false; }
    virtual bool isInteractive() const { return isFocusable(); }
    virtual bool isFocused() const { return false; }
    virtual void setFocused(bool focused) {}
    virtual bool hasMouseCapture() const { return false; }
    virtual bool hasOverlay() const { return false; }
    virtual void drawOverlay() {}

    virtual bool containsPoint(int x, int y) const {
        SDL_Point point = {x, y};
        return SDL_PointInRect(&point, &rect) == SDL_TRUE;
    }

    void changeRect(int x, int y, int w, int h) {
        this->rect = {x, y, w, h};
    }
};
