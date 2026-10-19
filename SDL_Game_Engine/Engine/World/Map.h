#pragma once

#include "../Math/Vector2.h"

#include <SDL.h>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

struct CameraComponent;

class Map {
public:
    math::Vector2 position{};
    float scale = 1.0f;

    bool load(const std::filesystem::path& jsonPath,
              const std::filesystem::path& imageOverride = {});
    void draw(SDL_Renderer* renderer, const CameraComponent* camera = nullptr) const;
    void clear() noexcept;

    bool isLoaded() const noexcept { return texture != nullptr; }
    int width() const noexcept { return imageWidth; }
    int height() const noexcept { return imageHeight; }
    int tileWidth() const noexcept { return cellWidth; }
    int tileHeight() const noexcept { return cellHeight; }
    int columns() const noexcept { return cellWidth ? imageWidth / cellWidth : 0; }
    int rows() const noexcept { return cellHeight ? imageHeight / cellHeight : 0; }
    std::size_t tileCount() const noexcept { return cellCount; }
    math::Vector2 worldSize() const noexcept { return {imageWidth * scale, imageHeight * scale}; }
    const std::string& error() const noexcept { return lastError; }

private:
    std::shared_ptr<SDL_Texture> texture;
    int imageWidth = 0, imageHeight = 0;
    int cellWidth = 0, cellHeight = 0;
    std::size_t cellCount = 0;
    std::string lastError;
};
