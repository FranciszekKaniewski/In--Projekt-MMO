#pragma once

#include "../Math/Vector2.h"

#include <SDL.h>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct CameraComponent;

class Map {
public:
    struct CollisionRun {
        int begin;
        int end;
    };
    using CollisionRuns = std::vector<std::vector<CollisionRun>>;

    math::Vector2 position{};
    float scale = 1.0f;

    bool load(const std::filesystem::path& jsonPath,
              const std::filesystem::path& imageOverride = {});
    bool loadTilemap(std::string_view json, const std::filesystem::path& localAtlas);
    math::Vector2 moveBox(math::Vector2 from, math::Vector2 destination, math::Vector2 halfSize) const;
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
    const CollisionRuns& collisionRows() const noexcept { return horizontalRuns; }
    const CollisionRuns& collisionColumns() const noexcept { return verticalRuns; }
    math::Vector2 worldSize() const noexcept { return {imageWidth * scale, imageHeight * scale}; }
    const std::string& error() const noexcept { return lastError; }

private:
    void setTilemap(std::string_view json, const std::filesystem::path& path,
                    const std::filesystem::path& imageOverride);
    std::shared_ptr<SDL_Texture> texture;
    int imageWidth = 0, imageHeight = 0;
    int cellWidth = 0, cellHeight = 0;
    std::size_t cellCount = 0;
    std::vector<bool> blockedTiles;
    CollisionRuns horizontalRuns;
    CollisionRuns verticalRuns;
    std::string lastError;
};
