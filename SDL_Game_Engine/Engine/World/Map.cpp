#include "Map.h"

#include "../Renderer/TextureManager.h"
#include "../ECS/Components/CameraComponent.h"
#include <nlohmann/json.hpp>

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using Json = nlohmann::json;

int integer(const Json& object, const char* key, int minimum) {
    const auto& value = object.at(key);
    if (!value.is_number_integer() || value < minimum || value > std::numeric_limits<int>::max()) {
        throw std::runtime_error(std::string("Invalid integer field: ") + key);
    }
    return value.get<int>();
}
}

bool Map::load(const std::filesystem::path& jsonPath, const std::filesystem::path& imageOverride) {
    try {
        std::ifstream input(jsonPath);
        if (!input) throw std::runtime_error("Cannot open the map JSON.");
        const auto data = Json::parse(input);
        const auto& meta = data.at("meta");
        const int width = integer(meta.at("size"), "w", 1);
        const int height = integer(meta.at("size"), "h", 1);
        const auto& frames = data.at("frames");
        if (!frames.is_array() || frames.empty()) {
            throw std::runtime_error("frames must be a nonempty spatial grid array.");
        }

        const int tileWidth = integer(frames.front().at("frame"), "w", 1);
        const int tileHeight = integer(frames.front().at("frame"), "h", 1);
        if (width % tileWidth || height % tileHeight) {
            throw std::runtime_error("Map size must be divisible by the tile dimensions.");
        }
        const int columns = width / tileWidth;
        const auto count = static_cast<std::uint64_t>(columns) * (height / tileHeight);
        if (count != frames.size()) {
            throw std::runtime_error("The spatial grid must cover every tile of the map image.");
        }

        std::vector<bool> occupied(frames.size(), false);
        for (const auto& entry : frames) {
            if (entry.value("rotated", false)) {
                throw std::runtime_error("Rotated atlas frames are not supported by this map format.");
            }
            const auto& frame = entry.at("frame");
            const int x = integer(frame, "x", 0);
            const int y = integer(frame, "y", 0);
            const int w = integer(frame, "w", 1);
            const int h = integer(frame, "h", 1);
            if (w != tileWidth || h != tileHeight || x % tileWidth || y % tileHeight ||
                x > width - tileWidth || y > height - tileHeight) {
                throw std::runtime_error("A tile is misaligned, has the wrong size or lies outside the map.");
            }
            const auto index = static_cast<std::size_t>(y / tileHeight) * columns + x / tileWidth;
            if (occupied[index]) throw std::runtime_error("Duplicate tile coordinates.");
            occupied[index] = true;
        }

        auto imagePath = imageOverride;
        if (imagePath.empty()) {
            if (meta.contains("image")) {
                imagePath = jsonPath.parent_path() / meta.at("image").get<std::string>();
            } else {
                imagePath = jsonPath;
                imagePath.replace_extension(".png");
            }
        }
        const auto imageName = imagePath.u8string();
        auto replacement = TextureManager::LoadSharedTexture(imageName.c_str());
        if (!replacement) throw std::runtime_error("Cannot load map image: " + imageName);
        int actualWidth = 0, actualHeight = 0;
        if (SDL_QueryTexture(replacement.get(), nullptr, nullptr, &actualWidth, &actualHeight) != 0 ||
            actualWidth != width || actualHeight != height) {
            throw std::runtime_error("Image dimensions do not match meta.size.");
        }
        if (SDL_SetTextureScaleMode(replacement.get(), SDL_ScaleModeNearest) != 0) {
            throw std::runtime_error(SDL_GetError());
        }

        texture = std::move(replacement);
        imageWidth = width;
        imageHeight = height;
        cellWidth = tileWidth;
        cellHeight = tileHeight;
        cellCount = frames.size();
        lastError.clear();
        return true;
    } catch (const std::exception& exception) {
        lastError = "Cannot load map '" + jsonPath.u8string() + "': " + exception.what();
        return false;
    }
}

void Map::draw(SDL_Renderer* renderer, const CameraComponent* camera) const {
    if (!renderer || !texture || scale <= 0.0f || !std::isfinite(scale) ||
        !std::isfinite(position.x) || !std::isfinite(position.y)) return;
    const auto size = worldSize();
    if (!std::isfinite(size.x) || !std::isfinite(size.y) ||
        !std::isfinite(position.x + size.x) || !std::isfinite(position.y + size.y)) return;

    SDL_FRect destination{position.x, position.y, size.x, size.y};
    if (!camera) {
        SDL_RenderCopyF(renderer, texture.get(), nullptr, &destination);
        return;
    }
    if (!camera->isVisible(destination)) return;

    // Request only the intersecting texels. Expand to whole source pixels so that
    // fractional camera movement does not stretch the edge texels or create gaps.
    const double left = std::max<double>(position.x, camera->position.x);
    const double top = std::max<double>(position.y, camera->position.y);
    const double right = std::min<double>(static_cast<double>(position.x) + size.x,
                                         static_cast<double>(camera->position.x) + camera->size.x);
    const double bottom = std::min<double>(static_cast<double>(position.y) + size.y,
                                          static_cast<double>(camera->position.y) + camera->size.y);
    const int sourceX = static_cast<int>(std::floor(std::clamp((left - position.x) / scale, 0.0, double(imageWidth))));
    const int sourceY = static_cast<int>(std::floor(std::clamp((top - position.y) / scale, 0.0, double(imageHeight))));
    const int endX = static_cast<int>(std::ceil(std::clamp((right - position.x) / scale, 0.0, double(imageWidth))));
    const int endY = static_cast<int>(std::ceil(std::clamp((bottom - position.y) / scale, 0.0, double(imageHeight))));
    const SDL_Rect source{sourceX, sourceY, endX - sourceX, endY - sourceY};
    if (source.w <= 0 || source.h <= 0) return;
    destination = {position.x + source.x * scale - camera->position.x,
                   position.y + source.y * scale - camera->position.y,
                   source.w * scale, source.h * scale};
    SDL_RenderCopyF(renderer, texture.get(), &source, &destination);
}

void Map::clear() noexcept {
    texture.reset();
    imageWidth = imageHeight = cellWidth = cellHeight = 0;
    cellCount = 0;
    lastError.clear();
}
