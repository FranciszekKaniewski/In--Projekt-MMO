#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "../Vendor/SDL2/include/SDL2/SDL_ttf.h"

class Font {
private:
    std::shared_ptr<TTF_Font> ownedFont;

    bool load(const std::string& path, int size) {
        if (path.empty() || size <= 0) {
            std::cerr << "Font path must not be empty and font size must be positive." << std::endl;
            return false;
        }

        TTF_Font* loadedFont = TTF_OpenFont(path.c_str(), size);
        if (!loadedFont) {
            std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
            return false;
        }

        std::shared_ptr<TTF_Font> replacement(loadedFont, TTF_CloseFont);
        filePath = path;
        fontSize = size;
        ownedFont = std::move(replacement);
        font = ownedFont.get();
        return true;
    }

public:
    TTF_Font* font = nullptr;
    std::string filePath;
    int fontSize = 16;
    SDL_Color color = {0, 0, 0, 255};

    Font() = default;

    Font(const char* filePath, int fontSize, SDL_Color color = {0,0,0,255}) :
        filePath(filePath ? filePath : ""), fontSize(fontSize), color(color) {
        load(this->filePath, this->fontSize);
    }

    Font(const Font&) = default;
    Font& operator=(const Font&) = default;

    Font(Font&& other) noexcept :
        ownedFont(std::move(other.ownedFont)),
        font(std::exchange(other.font, nullptr)), filePath(std::move(other.filePath)),
        fontSize(std::exchange(other.fontSize, 16)), color(other.color) {
        other.filePath.clear();
    }

    Font& operator=(Font&& other) noexcept {
        if (this != &other) {
            ownedFont = std::move(other.ownedFont);
            font = std::exchange(other.font, nullptr);
            filePath = std::move(other.filePath);
            fontSize = std::exchange(other.fontSize, 16);
            color = other.color;
            other.filePath.clear();
        }
        return *this;
    }

    bool changeSize(int newSize) {
        return load(filePath, newSize);
    }

    bool changeFontStyle(const char* newFilePath) {
        return load(newFilePath ? newFilePath : "", fontSize);
    }

    ~Font() = default;
};
