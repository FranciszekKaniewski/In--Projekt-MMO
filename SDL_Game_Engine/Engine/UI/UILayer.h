#pragma once
#include "vector"
#include <algorithm>
#include "../Core/App.h"
#include "./UIBox.h"

class UILayer {
private:
    std::vector<UIBox*> boxes;
    App& app;

public:
    UILayer(App& app) : app(app){};

    void handleEvents(){
        int x, y;
        SDL_GetMouseState(&x, &y);

        if (app.event.type == SDL_MOUSEMOTION) {
            x = app.event.motion.x;
            y = app.event.motion.y;
        } else if (app.event.type == SDL_MOUSEBUTTONDOWN ||
                   app.event.type == SDL_MOUSEBUTTONUP) {
            x = app.event.button.x;
            y = app.event.button.y;
        }

        for(UIBox* box : boxes){
            box->updateMousePosClick(x,y);
        }

        if (app.event.type == SDL_MOUSEBUTTONDOWN &&
            app.event.button.button == SDL_BUTTON_LEFT) {
            for(UIBox* box : boxes){
                box->handleClick();
            }
        }
    }

    void render(){
        for(UIBox* box : boxes){
            box->draw();
        }
    }

    UIBox* addBox(SDL_Rect rect, SDL_Color color={255,255,255,255},int cornerRadius=0, std::string name="", bool isClosed=false){
        int windowWith;
        int windowHeight;

        SDL_GetWindowSize(app.window, &windowWith, &windowHeight);

        if(rect.x == -1)
            rect.x = windowWith/2 - rect.w/2;
        if(rect.y == -1)
            rect.y = windowHeight/2 - rect.h/2;

        std::string finalName;
        name != "" ? finalName = name :
        finalName = "Box" + std::to_string(boxes.size());

        UIBox* box = new UIBox(finalName, rect, color, cornerRadius, isClosed);
        boxes.push_back(box);
        return box;
    }

    void removeWindow(UIBox* box) {
        if(!box) return;

        auto it = std::find(boxes.begin(), boxes.end(), box);
        if(it != boxes.end()){
            (*it)->clean();
            delete *it;
            boxes.erase(it);
        }
    }

    UIBox* getBoxByIndex(int index){
        return boxes[index];
    }

    void clean(){
        for(UIBox* box : boxes){
            box->clean();
        }
        boxes.clear();
    }
};
