#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include "./UIElement.h"
#include "../Core/Font.h"
#include "../Core/App.h"
#include "../Renderer/TextureManager.h"

class Input : public UIElement {
private:
    Font font;
    std::string text;
    std::string placeholder;

    SDL_Texture* texture = nullptr;

    std::size_t cursor = 0;

    int scrollX = 0;
    bool focused = false;
    Uint32 blinkStart = 0;

    static bool isContinuation(char c) {
        return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
    }

    std::size_t previousCharacter(std::size_t position) const {
        if (position == 0) return 0;

        do {
            --position;
        } while (position > 0 && isContinuation(text[position]));

        return position;
    }

    std::size_t nextCharacter(std::size_t position) const {
        if (position >= text.size()) return text.size();

        do {
            ++position;
        } while (position < text.size() &&
                 isContinuation(text[position]));

        return position;
    }

    static std::string singleLine(std::string value) {
        value.erase(
                std::remove_if(value.begin(), value.end(), [](char c) {
                    return c == '\n' || c == '\r' || c == '\t';
                }),
                value.end()
        );

        return value;
    }

    void rebuildTexture() {
        SDL_DestroyTexture(texture);

        Font drawFont = font;

        if (text.empty()) {
            drawFont.color = placeholderColor;
        }

        texture = TextureManager::LoadTextTexture(
                drawFont,
                text.empty() ? placeholder : text
        );
    }

    void textChanged() {
        rebuildTexture();
        blinkStart = SDL_GetTicks();

        if (onChange) {
            onChange(text);
        }
    }

    int cursorWidth() const {
        if (!font.font || cursor == 0) return 0;

        int width = 0;
        int height = 0;

        const std::string prefix = text.substr(0, cursor);
        TTF_SizeUTF8(font.font, prefix.c_str(), &width, &height);

        return width;
    }

public:
    int padding = 10;
    int cornerRadius = 8;

    SDL_Color bgColor = {255, 255, 255, 255};
    SDL_Color borderColor = {170, 170, 170, 255};
    SDL_Color focusColor = {60, 120, 220, 255};
    SDL_Color placeholderColor = {140, 140, 140, 255};

    std::function<void(const std::string&)> onChange;
    std::function<void(const std::string&)> onSubmit;

    Input(
            Font font,
            const std::string& placeholder = "",
            SDL_Rect rect = {0, 0, 280, 48}
    )
            : UIElement(rect),
              font(std::move(font)),
              placeholder(placeholder) {
        rebuildTexture();
    }

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    ~Input() override {
        clean();
    }

    const std::string& getText() const {
        return text;
    }

    void setText(const std::string& value) {
        const std::string newText = singleLine(value);

        if (text == newText) return;

        text = newText;
        cursor = text.size();
        scrollX = 0;

        textChanged();
    }

    bool containsPoint(int x, int y) const {
        SDL_Point point = {x, y};
        return SDL_PointInRect(&point, &rect) == SDL_TRUE;
    }

    bool isFocused() const {
        return focused;
    }

    void setFocused(bool value) {
        if (focused == value) return;

        focused = value;
        blinkStart = SDL_GetTicks();

        if (focused) {
            cursor = text.size();

            SDL_SetTextInputRect(&rect);
            SDL_StartTextInput();
        } else {
            SDL_StopTextInput();
        }
    }

    bool handleEvent(const SDL_Event& event) {
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            setFocused(false);
            return false;
        }

        if (!focused) return false;

        if (event.type == SDL_TEXTINPUT) {
            const std::string entered = singleLine(event.text.text);

            if (!entered.empty()) {
                text.insert(cursor, entered);
                cursor += entered.size();

                textChanged();
            }

            return true;
        }

        if (event.type == SDL_KEYUP) return true;
        if (event.type != SDL_KEYDOWN) return false;

        blinkStart = SDL_GetTicks();

        switch (event.key.keysym.sym) {
            case SDLK_BACKSPACE:
                if (cursor > 0) {
                    const auto previous = previousCharacter(cursor);

                    text.erase(previous, cursor - previous);
                    cursor = previous;

                    textChanged();
                }
                break;

            case SDLK_DELETE:
                if (cursor < text.size()) {
                    text.erase(
                            cursor,
                            nextCharacter(cursor) - cursor
                    );

                    textChanged();
                }
                break;

            case SDLK_LEFT:
                cursor = previousCharacter(cursor);
                break;

            case SDLK_RIGHT:
                cursor = nextCharacter(cursor);
                break;

            case SDLK_HOME:
                cursor = 0;
                break;

            case SDLK_END:
                cursor = text.size();
                break;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (!event.key.repeat && onSubmit) {
                    onSubmit(text);
                }
                break;

            case SDLK_ESCAPE:
                setFocused(false);
                break;

            default:
                break;
        }

        return true;
    }

    void draw() override {
        if (!App::renderer || rect.w <= 4 || rect.h <= 4) return;

        TextureManager::DrawRectangle(
                rect,
                focused ? focusColor : borderColor,
                cornerRadius
        );

        SDL_Rect background = {
                rect.x + 2,
                rect.y + 2,
                rect.w - 4,
                rect.h - 4
        };

        TextureManager::DrawRectangle(
                background,
                bgColor,
                std::max(0, cornerRadius - 2)
        );

        const int inset = std::max(2, padding);

        SDL_Rect content = {
                rect.x + inset,
                rect.y + 2,
                rect.w - 2 * inset,
                rect.h - 4
        };

        if (content.w <= 2 || content.h <= 0 || !font.font) return;

        int width = 0;
        int height = TTF_FontHeight(font.font);

        if (texture) {
            SDL_QueryTexture(
                    texture, nullptr, nullptr, &width, &height
            );
        }

        const int caretX = cursorWidth();
        const int visibleWidth = content.w - 2;

        if (caretX < scrollX) {
            scrollX = caretX;
        }

        if (caretX > scrollX + visibleWidth) {
            scrollX = caretX - visibleWidth;
        }

        scrollX = std::clamp(
                scrollX,
                0,
                std::max(0, width - visibleWidth)
        );

        SDL_Rect oldClip;

        const bool hadClip =
                SDL_RenderIsClipEnabled(App::renderer) == SDL_TRUE;

        SDL_RenderGetClipRect(App::renderer, &oldClip);

        SDL_Rect clip = content;

        if (hadClip &&
            !SDL_IntersectRect(&oldClip, &content, &clip)) {
            return;
        }

        if (SDL_RenderSetClipRect(App::renderer, &clip) != 0) {
            return;
        }

        const int textY = content.y + (content.h - height) / 2;

        if (texture) {
            SDL_Rect destination = {
                    content.x - scrollX,
                    textY,
                    width,
                    height
            };

            SDL_RenderCopy(
                    App::renderer, texture, nullptr, &destination
            );
        }

        if (focused &&
            ((SDL_GetTicks() - blinkStart) / 500) % 2 == 0) {
            SDL_Rect caret = {
                    content.x + caretX - scrollX,
                    textY,
                    2,
                    height
            };

            TextureManager::DrawRectangle(caret, font.color);
        }

        SDL_RenderSetClipRect(
                App::renderer,
                hadClip ? &oldClip : nullptr
        );
    }

    void clean() override {
        setFocused(false);

        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
};