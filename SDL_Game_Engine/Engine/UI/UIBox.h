#pragma once
#include "string"
#include "vector"
#include <functional>
#include <utility>

#include "../Vendor/SDL2/include/SDL2/SDL.h"
#include "../Core/App.h"
#include "./Label.h"
#include "./Button.h"
#include "./Input.h"

class UIBox{
private:
    std::string name;
    SDL_Rect rect;
    SDL_Color bgColor;
    bool isClosed;
    int cornerRadius;

    std::vector<UIElement*> elements;

public:
    UIBox(std::string name, SDL_Rect rect, SDL_Color bgColor = {255,255,255,255}, int cornerRadius = 0, bool isClosed = false) :
    name(name), rect(rect), bgColor(bgColor), isClosed(isClosed), cornerRadius(cornerRadius) {};

    UIBox(const UIBox&) = delete;
    UIBox& operator=(const UIBox&) = delete;

    ~UIBox() {
        clean();
    }

    void setCornerRadius(int radius) {
        cornerRadius = radius > 0 ? radius : 0;
    }

    void draw() {
        TextureManager::DrawRectangle(rect, bgColor, cornerRadius);
        for(UIElement* e : elements) e->draw();
    }

    void updateMousePosClick(int x,int y) {
        if(isClosed) return;
        for(UIElement* e : elements){
            if (x >= e->rect.x && x <= e->rect.x + e->rect.w &&
                y >= e->rect.y && y <= e->rect.y + e->rect.h) {

                e->setHover(true);
            }else{
                e->setHover(false);
            }
        }
    }

    void handleClick(){
        if(isClosed) return;
        for(UIElement* e : elements) e->handleClick();
    }

    Input* getInputAtPoint(int x, int y) const {
        if(isClosed) return nullptr;

        for(auto it = elements.rbegin(); it != elements.rend(); ++it){
            Input* input = dynamic_cast<Input*>(*it);
            if(input && input->containsPoint(x, y)) return input;
        }

        return nullptr;
    }

    void clean(){
        for(UIElement* e : elements){
            e->clean();
            delete e;
        }
        elements.clear();
    }

    void addLabel(Font font,const std::string& text, SDL_Color color={0,0,0,0}, SDL_Rect rect={0,0,128,64}){
        if(rect.x == -1)
            rect.x = this->rect.w/2 - rect.w/2;
        if(rect.y == -1)
            rect.y = this->rect.h/2 - rect.h/2;
        SDL_Rect windowedRect = {rect.x+this->rect.x,rect.y+this->rect.y,rect.w,rect.h};

        Label* label = new Label(font,text,color,windowedRect);
        elements.push_back(label);
    }

    Button* addButton(Font font, const std::string& text,
                      SDL_Color color = {60,120,220,255}, SDL_Rect rect = {0,0,128,64},
                      float hoverScale = 1.05f,
                      std::function<void()> onClick = {}) {
        if(rect.x == -1)
            rect.x = this->rect.w/2 - rect.w/2;
        if(rect.y == -1)
            rect.y = this->rect.h/2 - rect.h/2;
        SDL_Rect windowedRect = {rect.x+this->rect.x,rect.y+this->rect.y,rect.w,rect.h};

        Button* button = new Button(font, text, color, windowedRect,
                                    hoverScale, std::move(onClick));
        elements.push_back(button);
        return button;
    }

    Button* addButton(Button* btn) {
        SDL_Rect windowedRect = btn->rect;
        if(btn->rect.x == -1)
            windowedRect.x = this->rect.w/2 - btn->rect.w/2;
        if(btn->rect.y == -1)
            windowedRect.y = this->rect.h/2 - btn->rect.h/2;
        btn->changeRect(this->rect.x+windowedRect.x,this->rect.y+windowedRect.y,windowedRect.w,windowedRect.h);

        elements.push_back(btn);
        return btn;
    }

    Input* addInput(Font font, const std::string& placeholder = "",
                    SDL_Rect rect = {0,0,280,48}) {
        return addInput(new Input(std::move(font), placeholder, rect));
    }

    Input* addInput(Input* input) {
        if(!input) return nullptr;

        SDL_Rect windowedRect = input->rect;
        if(windowedRect.x == -1)
            windowedRect.x = this->rect.w/2 - windowedRect.w/2;
        if(windowedRect.y == -1)
            windowedRect.y = this->rect.h/2 - windowedRect.h/2;

        input->changeRect(this->rect.x + windowedRect.x,
                          this->rect.y + windowedRect.y,
                          windowedRect.w, windowedRect.h);
        elements.push_back(input);
        return input;
    }
};
