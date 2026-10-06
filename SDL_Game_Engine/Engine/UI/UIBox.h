#pragma once
#include "string"
#include "vector"

#include "../Vendor/SDL2/include/SDL2/SDL.h"
#include "../Core/App.h"
#include "./Label.h"

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
        for(UIElement* e : elements) e->handleClick();
    }

    void clean(){
        for(UIElement* e : elements) e->clean();
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
};