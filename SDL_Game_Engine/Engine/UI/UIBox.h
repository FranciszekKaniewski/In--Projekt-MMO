#include "string"
#include "vector"

#include "../Vendor/SDL2/include/SDL2/SDL.h"
#include "./UIElement.h"
#include "../Core/App.h"

class UIBox{
private:
    std::string name;
    SDL_Rect rect;
    SDL_Color bgColor;
    bool isClosed;

    std::vector<UIElement*> elements;

public:
    UIBox(std::string name, SDL_Rect rect, SDL_Color bgColor = {255,255,255,255}, bool isClosed = false) :
    name(name), rect(rect), bgColor(bgColor), isClosed(isClosed) {};

    void draw() {
        Uint8 prevR, prevG, prevB, prevA;

        SDL_GetRenderDrawColor(App::renderer, &prevR, &prevG, &prevB, &prevA);
        SDL_SetRenderDrawColor(App::renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderFillRect(App::renderer, &rect);
        SDL_SetRenderDrawColor(App::renderer, prevR, prevG, prevB, prevA);
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
};