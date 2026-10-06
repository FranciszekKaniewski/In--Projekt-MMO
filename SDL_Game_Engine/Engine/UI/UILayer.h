#pragma once
#include "vector"
#include <algorithm>
#include "../Core/App.h"
#include "./UIBox.h"

class UILayer {
private:
    std::vector<UIBox*> boxes;
    App& app;
    Input* focusedInput = nullptr;
    UIBox* focusedBox = nullptr;

    void focusInput(Input* input, UIBox* box = nullptr) {
        if(focusedInput == input) return;

        if(focusedInput) focusedInput->setFocused(false);
        focusedInput = input;
        focusedBox = box;
        if(focusedInput) focusedInput->setFocused(true);
    }

public:
    UILayer(App& app) : app(app){};

    UILayer(const UILayer&) = delete;
    UILayer& operator=(const UILayer&) = delete;

    ~UILayer() {
        clean();
    }

    bool handleEvents(){
        return handleEvents(app.event);
    }

    bool handleEvents(const SDL_Event& event){
        if(event.type == SDL_WINDOWEVENT &&
           event.window.event == SDL_WINDOWEVENT_FOCUS_LOST){
            focusInput(nullptr);
            return false;
        }

        int x, y;
        SDL_GetMouseState(&x, &y);

        if (event.type == SDL_MOUSEMOTION) {
            x = event.motion.x;
            y = event.motion.y;
        } else if (event.type == SDL_MOUSEBUTTONDOWN ||
                   event.type == SDL_MOUSEBUTTONUP) {
            x = event.button.x;
            y = event.button.y;
        }

        for(UIBox* box : boxes){
            box->updateMousePosClick(x,y);
        }

        if (event.type == SDL_MOUSEBUTTONDOWN &&
            event.button.button == SDL_BUTTON_LEFT) {
            Input* clickedInput = nullptr;
            UIBox* clickedBox = nullptr;

            for(auto it = boxes.rbegin(); it != boxes.rend(); ++it){
                clickedInput = (*it)->getInputAtPoint(x, y);
                if(clickedInput){
                    clickedBox = *it;
                    break;
                }
            }

            focusInput(clickedInput, clickedBox);
            if(clickedInput) return true;

            for(UIBox* box : boxes){
                box->handleClick();
            }
        }

        if(focusedInput){
            const bool handled = focusedInput->handleEvent(event);
            if(focusedInput && !focusedInput->isFocused()) focusInput(nullptr);
            return handled;
        }

        return false;
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
            if(focusedBox == box) focusInput(nullptr);
            delete *it;
            boxes.erase(it);
        }
    }

    UIBox* getBoxByIndex(int index){
        return boxes[index];
    }

    void clean(){
        focusInput(nullptr);
        for(UIBox* box : boxes){
            delete box;
        }
        boxes.clear();
    }
};
