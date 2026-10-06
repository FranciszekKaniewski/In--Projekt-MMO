#pragma once
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
        UIElement(rect), font(font), text(text),bgColor(bgColor) {
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

        SDL_Rect textRect = {rect.x, rect.y,rect.w, rect.h};
        SDL_RenderCopy(App::renderer, texture, nullptr, &textRect);
    }

    void clean() override {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
};
