#pragma once
#include "vector"
#include <algorithm>
#include "../Core/App.h"
#include "./UIBox.h"

class UILayer {
private:
    std::vector<UIBox*> boxes;
    App& app;
    UIElement* focusedElement = nullptr;
    UIBox* focusedBox = nullptr;
    UIElement* capturedElement = nullptr;

    void releaseMouseCapture() {
        if(capturedElement) SDL_CaptureMouse(SDL_FALSE);
        capturedElement = nullptr;
    }

    void focusElement(UIElement* element, UIBox* box = nullptr) {
        if(focusedElement == element) return;

        if(focusedElement) focusedElement->setFocused(false);
        releaseMouseCapture();
        focusedElement = element;
        focusedBox = box;
        if(focusedElement) focusedElement->setFocused(true);
    }

    void cycleFocus(bool backwards) {
        std::vector<std::pair<UIElement*, UIBox*>> targets;
        for(UIBox* box : boxes)
            for(UIElement* element : box->getFocusableElements())
                targets.emplace_back(element, box);
        if(targets.empty()) return;

        auto current = std::find_if(targets.begin(), targets.end(),
            [this](const auto& target){ return target.first == focusedElement; });
        int index = backwards ? static_cast<int>(targets.size()) - 1 : 0;
        if(current != targets.end()) {
            const int count = static_cast<int>(targets.size());
            index = (static_cast<int>(current - targets.begin()) +
                     (backwards ? count - 1 : 1)) % count;
        }
        focusElement(targets[index].first, targets[index].second);
    }

    bool dispatchFocused(const SDL_Event& event) {
        if(!focusedElement) return false;
        UIElement* target = focusedElement;
        const bool handled = target->handleEvent(event);
        if(focusedElement == target && !target->isFocused()) focusElement(nullptr);
        return handled;
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
            focusElement(nullptr);
            releaseMouseCapture();
            return false;
        }

        if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB) {
            if(!event.key.repeat) cycleFocus((event.key.keysym.mod & KMOD_SHIFT) != 0);
            return true;
        }

        int x = 0, y = 0;
        const bool pointerEvent = event.type == SDL_MOUSEMOTION ||
            event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP;

        if (event.type == SDL_MOUSEMOTION) {
            x = event.motion.x;
            y = event.motion.y;
        } else if (event.type == SDL_MOUSEBUTTONDOWN ||
                   event.type == SDL_MOUSEBUTTONUP) {
            x = event.button.x;
            y = event.button.y;
        }

        if(pointerEvent) {
            for(UIBox* box : boxes) box->updateMousePosClick(x,y);
        }

        if(capturedElement && (event.type == SDL_MOUSEMOTION ||
                               event.type == SDL_MOUSEBUTTONUP)) {
            UIElement* target = capturedElement;
            const bool handled = target->handleEvent(event);
            if(capturedElement == target && !target->hasMouseCapture()) releaseMouseCapture();
            return handled;
        }

        // A dropdown receives pointer events before controls underneath it.
        if(focusedElement && focusedElement->hasOverlay() &&
           (pointerEvent || event.type == SDL_MOUSEWHEEL)) {
            if(dispatchFocused(event)) return true;
        }

        if (event.type == SDL_MOUSEBUTTONDOWN &&
            event.button.button == SDL_BUTTON_LEFT) {
            UIElement* clickedElement = nullptr;
            UIBox* clickedBox = nullptr;

            for(auto it = boxes.rbegin(); it != boxes.rend(); ++it){
                clickedElement = (*it)->getElementAtPoint(x, y);
                if(clickedElement){
                    clickedBox = *it;
                    break;
                }
            }

            focusElement(clickedElement && clickedElement->isFocusable() ? clickedElement : nullptr,
                         clickedElement && clickedElement->isFocusable() ? clickedBox : nullptr);
            if(!clickedElement) return false;

            const bool handled = clickedElement->handleEvent(event);
            if(focusedElement == clickedElement && clickedElement->hasMouseCapture()) {
                capturedElement = clickedElement;
                SDL_CaptureMouse(SDL_TRUE);
            }
            if(!handled) clickedElement->handleClick();
            return true;
        }

        return dispatchFocused(event);
    }

    void render(){
        for(UIBox* box : boxes){
            box->draw();
        }
        if(focusedElement && focusedElement->hasOverlay()) focusedElement->drawOverlay();
    }

    UIBox* addBox(SDL_Rect rect, SDL_Color color={255,255,255,255},int cornerRadius=0, std::string name="",
                  bool isClosed=false, const std::string& backgroundImage = ""){
        int windowWith;
        int windowHeight;

        const WindowSize size = app.getUISize();
        windowWith = size.wight;
        windowHeight = size.height;

        if(rect.x == -1)
            rect.x = windowWith/2 - rect.w/2;
        if(rect.y == -1)
            rect.y = windowHeight/2 - rect.h/2;

        std::string finalName;
        name != "" ? finalName = name :
        finalName = "Box" + std::to_string(boxes.size());

        UIBox* box = new UIBox(finalName, rect, color, cornerRadius, isClosed, backgroundImage);
        boxes.push_back(box);
        return box;
    }

    void cleanBox(UIBox* box) {
        if(!box) return;

        auto it = std::find(boxes.begin(), boxes.end(), box);

        if (it != boxes.end()) {
            if (focusedBox == box) {
                focusElement(nullptr);
            }

            (*it)->clean();
        }
    }

    void removeBox(UIBox* box) {
        if(!box) return;

        auto it = std::find(boxes.begin(), boxes.end(), box);
        if(it != boxes.end()){
            if(focusedBox == box) focusElement(nullptr);
            delete *it;
            boxes.erase(it);
        }
    }

    UIBox* getBoxByIndex(int index){
        return boxes[index];
    }

    void clean(){
        focusElement(nullptr);
        releaseMouseCapture();
        for(UIBox* box : boxes){
            delete box;
        }
        boxes.clear();
    }
};
