#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>
#include "./Label.h"

class Select : public Label {
private:
    std::vector<std::string> options;
    std::vector<SDL_Texture*> optionTextures;
    int selectedIndex = -1;
    int highlightedIndex = -1;
    int firstVisible = 0;
    bool focused = false;
    bool hovered = false;
    bool open = false;

    SDL_Rect popupRect() const {
        const int rowHeight = std::max(1, rect.h);
        int rows = std::min(static_cast<int>(options.size()), std::max(1, maxVisibleRows));
        int viewportWidth = 0, viewportHeight = 0;
        if(App::renderer) {
            SDL_RenderGetLogicalSize(App::renderer, &viewportWidth, &viewportHeight);
            if(viewportHeight <= 0)
                SDL_GetRendererOutputSize(App::renderer, &viewportWidth, &viewportHeight);
        }
        const int below = std::max(0, viewportHeight - rect.y - rect.h - 4);
        const int above = std::max(0, rect.y - 4);
        const bool upwards = viewportHeight > 0 && rows * rowHeight > below && above > below;
        if(viewportHeight > 0) {
            rows = std::min(rows, std::max(1, (upwards ? above : below) / rowHeight));
        }
        const int height = rows * rowHeight;
        return {rect.x, upwards ? rect.y - height - 4 : rect.y + rect.h + 4,
                rect.w, height};
    }

    void ensureHighlightVisible() {
        const int rows = std::max(1, popupRect().h / std::max(1, rect.h));
        if(highlightedIndex >= 0) {
            if(highlightedIndex < firstVisible) firstVisible = highlightedIndex;
            if(highlightedIndex >= firstVisible + rows)
                firstVisible = highlightedIndex - rows + 1;
        }
        firstVisible = std::clamp(firstVisible, 0,
            std::max(0, static_cast<int>(options.size()) - rows));
    }

    void drawAlignedText(SDL_Texture* textTexture, SDL_Rect bounds) {
        if(!textTexture || bounds.w <= 0 || bounds.h <= 0) return;
        int width = 0, height = 0;
        if(SDL_QueryTexture(textTexture, nullptr, nullptr, &width, &height) != 0 ||
           width <= 0 || height <= 0) return;
        const double scale = std::min({1.0, static_cast<double>(bounds.w) / width,
                                      static_cast<double>(bounds.h) / height});
        SDL_Rect destination = {bounds.x, 0,
            std::max(1, static_cast<int>(std::lround(width * scale))),
            std::max(1, static_cast<int>(std::lround(height * scale)))};
        destination.y = bounds.y + (bounds.h - destination.h) / 2;
        SDL_RenderCopy(App::renderer, textTexture, nullptr, &destination);
    }

public:
    int maxVisibleRows = 5;
    SDL_Color borderColor = {170,170,170,255};
    SDL_Color focusColor = {60,120,220,255};
    SDL_Color highlightColor = {231,239,255,255};
    std::function<void(int, const std::string&)> onChange;

    Select(Font font, std::vector<std::string> options = {}, int selectedIndex = 0,
           SDL_Rect rect = {0,0,240,48},
           std::function<void(int, const std::string&)> onChange = {})
        : Label(font, "", {255,255,255,255}, rect), options(std::move(options)),
          onChange(std::move(onChange)) {
        bgCornerRadius = 8;
        for(const auto& option : this->options)
            optionTextures.push_back(TextureManager::LoadTextTexture(font, option));
        if(!this->options.empty()) {
            this->selectedIndex = std::clamp(selectedIndex, 0,
                static_cast<int>(this->options.size()) - 1);
            updateText(this->options[this->selectedIndex]);
        }
    }

    Select(const Select&) = delete;
    Select& operator=(const Select&) = delete;
    ~Select() override { clean(); }

    int getSelectedIndex() const { return selectedIndex; }
    const std::string& getSelectedText() const { return text; }
    const std::vector<std::string>& getOptions() const { return options; }
    bool isOpen() const { return open; }

    void setSelectedIndex(int index) {
        if(options.empty()) return;
        index = std::clamp(index, 0, static_cast<int>(options.size()) - 1);
        if(index == selectedIndex) return;
        selectedIndex = index;
        highlightedIndex = index;
        updateText(options[index]);
        ensureHighlightVisible();
        if(onChange) onChange(selectedIndex, text);
    }

    void setHover(bool hover) override { hovered = hover; }
    bool isFocusable() const override { return !options.empty(); }
    bool isFocused() const override { return focused; }
    bool hasOverlay() const override { return open && !options.empty(); }

    void setFocused(bool focus) override {
        focused = focus && !options.empty();
        if(!focused) open = false;
    }

    bool handleEvent(const SDL_Event& event) override {
        if(!focused) return false;

        if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            if(containsPoint(event.button.x, event.button.y)) {
                open = !open;
                highlightedIndex = selectedIndex;
                ensureHighlightVisible();
                return true;
            }
            if(open) {
                const SDL_Rect popup = popupRect();
                const SDL_Point point = {event.button.x, event.button.y};
                open = false;
                if(SDL_PointInRect(&point, &popup)) {
                    const int index = firstVisible + (point.y - popup.y) / std::max(1, rect.h);
                    setSelectedIndex(index);
                }
                return true;
            }
        }
        if(open && event.type == SDL_MOUSEMOTION) {
            const SDL_Rect popup = popupRect();
            const SDL_Point point = {event.motion.x, event.motion.y};
            highlightedIndex = SDL_PointInRect(&point, &popup) ?
                firstVisible + (point.y - popup.y) / std::max(1, rect.h) : -1;
            return true;
        }
        if(open && event.type == SDL_MOUSEWHEEL) {
            const int rows = std::max(1, popupRect().h / std::max(1, rect.h));
            const int direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
            firstVisible = std::clamp(firstVisible - event.wheel.y * direction, 0,
                std::max(0, static_cast<int>(options.size()) - rows));
            highlightedIndex = -1;
            return true;
        }
        if(event.type == SDL_KEYUP) return true;
        if(event.type != SDL_KEYDOWN) return false;

        const int last = static_cast<int>(options.size()) - 1;
        switch(event.key.keysym.sym) {
            case SDLK_SPACE:
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if(event.key.repeat) return true;
                if(open) {
                    open = false;
                    if(highlightedIndex >= 0) setSelectedIndex(highlightedIndex);
                } else {
                    open = true;
                    highlightedIndex = selectedIndex;
                    ensureHighlightVisible();
                }
                return true;
            case SDLK_UP:
            case SDLK_DOWN:
            case SDLK_HOME:
            case SDLK_END: {
                int index = open && highlightedIndex >= 0 ? highlightedIndex : selectedIndex;
                if(event.key.keysym.sym == SDLK_HOME) index = 0;
                else if(event.key.keysym.sym == SDLK_END) index = last;
                else index += event.key.keysym.sym == SDLK_UP ? -1 : 1;
                index = std::clamp(index, 0, last);
                if(open) {
                    highlightedIndex = index;
                    ensureHighlightVisible();
                } else setSelectedIndex(index);
                return true;
            }
            case SDLK_ESCAPE:
                if(open) open = false;
                else setFocused(false);
                return true;
            default: return false;
        }
    }

    void draw() override {
        if(!App::renderer || rect.w <= 4 || rect.h <= 4) return;
        TextureManager::DrawRectangle(rect,
            focused || hovered ? focusColor : borderColor, bgCornerRadius);
        SDL_Rect inner = {rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4};
        TextureManager::DrawRectangle(inner, bgColor, std::max(0, bgCornerRadius - 2));
        drawAlignedText(texture, {rect.x + 12, rect.y + 4, rect.w - 48, rect.h - 8});

        Uint8 r, g, b, a;
        SDL_GetRenderDrawColor(App::renderer, &r, &g, &b, &a);
        SDL_SetRenderDrawColor(App::renderer, borderColor.r, borderColor.g,
                               borderColor.b, borderColor.a);
        const int x = rect.x + rect.w - 22, y = rect.y + rect.h / 2;
        const int direction = open ? -1 : 1;
        SDL_RenderDrawLine(App::renderer, x - 5, y - direction * 2, x, y + direction * 3);
        SDL_RenderDrawLine(App::renderer, x, y + direction * 3, x + 5, y - direction * 2);
        SDL_SetRenderDrawColor(App::renderer, r, g, b, a);
    }

    void drawOverlay() override {
        if(!App::renderer || !hasOverlay() || rect.w <= 4 || rect.h <= 4) return;
        ensureHighlightVisible();
        const SDL_Rect popup = popupRect();
        TextureManager::DrawRectangle(popup, borderColor, bgCornerRadius);
        SDL_Rect inner = {popup.x + 2, popup.y + 2, popup.w - 4, popup.h - 4};
        TextureManager::DrawRectangle(inner, bgColor, std::max(0, bgCornerRadius - 2));
        const int rows = popup.h / rect.h;
        for(int row = 0; row < rows; ++row) {
            const int index = firstVisible + row;
            SDL_Rect bounds = {popup.x + 2, popup.y + row * rect.h + 2,
                               popup.w - 4, rect.h - 4};
            if(index == highlightedIndex)
                TextureManager::DrawRectangle(bounds, highlightColor, 4);
            bounds.x += 10;
            bounds.w -= 20;
            drawAlignedText(optionTextures[index], bounds);
        }
    }

    void clean() override {
        setFocused(false);
        for(SDL_Texture* option : optionTextures) SDL_DestroyTexture(option);
        optionTextures.clear();
        Label::clean();
    }
};
