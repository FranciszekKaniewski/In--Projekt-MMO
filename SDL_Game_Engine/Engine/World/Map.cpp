#include "Map.h"

#include "../Core/App.h"
#include "../Renderer/TextureManager.h"
#include "../ECS/Components/CameraComponent.h"
#include <SDL_image.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
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
        std::ifstream input(jsonPath, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open the map JSON.");
        if (jsonPath.extension() == ".tmj") {
            const std::string contents((std::istreambuf_iterator<char>(input)), {});
            setTilemap(contents, jsonPath, imageOverride);
            return true;
        }
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
        blockedTiles.clear();
        lastError.clear();
        return true;
    } catch (const std::exception& exception) {
        lastError = "Cannot load map '" + jsonPath.u8string() + "': " + exception.what();
        return false;
    }
}

bool Map::loadTilemap(std::string_view json, const std::filesystem::path& localAtlas) {
    try {
        if (localAtlas.empty()) throw std::runtime_error("A local tileset is required.");
        setTilemap(json, {}, localAtlas);
        return true;
    } catch (const std::exception& exception) {
        lastError = exception.what();
        return false;
    }
}

void Map::setTilemap(std::string_view json, const std::filesystem::path& path,
                     const std::filesystem::path& imageOverride) {
    const auto data = Json::parse(json);
    if (data.value("type", "") != "map" || data.value("orientation", "") != "orthogonal" ||
        data.value("infinite", false)) {
        throw std::runtime_error("Expected a finite orthogonal Tiled map.");
    }
    const int columns = integer(data, "width", 1);
    const int rows = integer(data, "height", 1);
    const int tileWidth = integer(data, "tilewidth", 1);
    const int tileHeight = integer(data, "tileheight", 1);
    constexpr int maxDimension = 4096;
    if (columns > maxDimension / tileWidth || rows > maxDimension / tileHeight) {
        throw std::runtime_error("Tilemap exceeds the supported image size.");
    }
    const int width = columns * tileWidth, height = rows * tileHeight;
    const auto count = static_cast<std::size_t>(columns) * rows;
    const auto& tilesets = data.at("tilesets");
    const auto& layers = data.at("layers");
    if (!tilesets.is_array() || tilesets.size() != 1 || !layers.is_array() ||
        layers.empty() || layers.size() > 16) {
        throw std::runtime_error("Expected one tileset and up to 16 tile layers.");
    }
    const auto& tileset = tilesets.front();
    if (tileset.contains("source") || integer(tileset, "firstgid", 1) != 1 ||
        integer(tileset, "tilewidth", 1) != tileWidth || integer(tileset, "tileheight", 1) != tileHeight ||
        tileset.value("spacing", 0) != 0 || tileset.value("margin", 0) != 0 || tileset.contains("tileoffset")) {
        throw std::runtime_error("Expected an embedded tileset with matching tiles and no spacing or offset.");
    }
    const auto imagePath = imageOverride.empty()
        ? path.parent_path() / std::filesystem::u8path(tileset.at("image").get<std::string>())
        : imageOverride;
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
    const Surface source(IMG_Load(imagePath.u8string().c_str()), SDL_FreeSurface);
    if (!source) throw std::runtime_error("Cannot load tileset: " + imagePath.u8string());
    const int atlasColumns = integer(tileset, "columns", 1);
    const int atlasCount = integer(tileset, "tilecount", 1);
    if (source->w != integer(tileset, "imagewidth", 1) || source->h != integer(tileset, "imageheight", 1) ||
        source->w % tileWidth || source->h % tileHeight || source->w / tileWidth != atlasColumns ||
        atlasCount > static_cast<std::int64_t>(atlasColumns) * (source->h / tileHeight)) {
        throw std::runtime_error("Tileset dimensions do not match the atlas.");
    }
    const Surface atlas(SDL_ConvertSurfaceFormat(source.get(), SDL_PIXELFORMAT_RGBA32, 0), SDL_FreeSurface);
    const Surface image(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface);
    if (!atlas || !image || SDL_FillRect(image.get(), nullptr, 0) != 0) throw std::runtime_error(SDL_GetError());

    std::vector<bool> collision;
    // Compose once so the camera can still draw one clipped texture per frame.
    for (const auto& layer : layers) {
        const auto opacity = layer.value("opacity", 1.0);
        if (layer.value("type", "") != "tilelayer" || integer(layer, "width", 1) != columns ||
            integer(layer, "height", 1) != rows || layer.value("x", 0) || layer.value("y", 0) ||
            layer.value("offsetx", 0.0) || layer.value("offsety", 0.0) ||
            layer.value("mode", "normal") != "normal" || !(opacity >= 0 && opacity <= 1)) {
            throw std::runtime_error("Expected aligned full-size tile layers with normal blending.");
        }
        const auto& gids = layer.at("data");
        if (!gids.is_array() || gids.size() != count) throw std::runtime_error("Invalid tile layer data.");
        const bool isCollision = layer.value("name", "") == "Collision";
        if (isCollision) {
            if (!collision.empty()) throw std::runtime_error("Expected at most one Collision layer.");
            collision.resize(count);
        }
        const bool visible = layer.value("visible", true);
        for (std::size_t i = 0; i < count; ++i) {
            const auto& value = gids[i];
            if (!value.is_number_integer() || value < 0 || value > atlasCount) {
                throw std::runtime_error("Invalid or unsupported tile ID.");
            }
            const int gid = value.get<int>();
            if (isCollision) {
                collision[i] = gid != 0;
                continue;
            }
            if (!gid || !visible || opacity == 0) continue;
            const int sourceX = (gid - 1) % atlasColumns * tileWidth;
            const int sourceY = (gid - 1) / atlasColumns * tileHeight;
            const int x = static_cast<int>(i % columns) * tileWidth;
            const int y = static_cast<int>(i / columns) * tileHeight;
            for (int dy = 0; dy < tileHeight; ++dy) {
                const auto* src = static_cast<const Uint8*>(atlas->pixels) + (sourceY + dy) * atlas->pitch + sourceX * 4;
                auto* dst = static_cast<Uint8*>(image->pixels) + (y + dy) * image->pitch + x * 4;
                for (int dx = 0; dx < tileWidth; ++dx, src += 4, dst += 4) {
                    const int alpha = static_cast<int>(std::lround(src[3] * opacity));
                    if (!alpha) continue;
                    const int foreground = alpha * 255;
                    const int background = dst[3] * (255 - alpha);
                    const int total = foreground + background;
                    for (int channel = 0; channel < 3; ++channel) {
                        dst[channel] = static_cast<Uint8>((src[channel] * foreground +
                            dst[channel] * background + total / 2) / total);
                    }
                    dst[3] = static_cast<Uint8>((total + 127) / 255);
                }
            }
        }
    }
    if (!App::renderer) throw std::runtime_error("Cannot load a tilemap without a renderer.");
    std::shared_ptr<SDL_Texture> replacement(SDL_CreateTextureFromSurface(App::renderer, image.get()), SDL_DestroyTexture);
    if (!replacement || SDL_SetTextureScaleMode(replacement.get(), SDL_ScaleModeNearest) != 0 ||
        SDL_SetTextureBlendMode(replacement.get(), SDL_BLENDMODE_BLEND) != 0) throw std::runtime_error(SDL_GetError());
    texture = std::move(replacement);
    imageWidth = width;
    imageHeight = height;
    cellWidth = tileWidth;
    cellHeight = tileHeight;
    cellCount = count;
    blockedTiles = std::move(collision);
    lastError.clear();
}

math::Vector2 Map::moveBox(math::Vector2 from, math::Vector2 destination, math::Vector2 halfSize) const {
    const double width = static_cast<double>(imageWidth) * scale;
    const double height = static_cast<double>(imageHeight) * scale;
    if (!isLoaded() || !std::isfinite(scale) || scale <= 0.0f ||
        !std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(from.x) || !std::isfinite(from.y) ||
        !std::isfinite(destination.x) || !std::isfinite(destination.y) ||
        !(halfSize.x > 0 && 2.0 * halfSize.x <= width &&
          halfSize.y > 0 && 2.0 * halfSize.y <= height)) return from;

    const auto local = [](float coordinate, float origin, float half, double size) {
        return std::clamp(static_cast<double>(coordinate) - origin, double(half), size - half);
    };
    const double targetX = local(destination.x, position.x, halfSize.x, width);
    const double targetY = local(destination.y, position.y, halfSize.y, height);
    if (blockedTiles.empty()) {
        return {static_cast<float>(position.x + targetX), static_cast<float>(position.y + targetY)};
    }

    const auto clipAxis = [this](double from, double to, double half,
                                 double acrossMin, double acrossMax, bool horizontal) {
        if (from == to) return to;
        const double cell = static_cast<double>(horizontal ? cellWidth : cellHeight) * scale;
        const double acrossCell = static_cast<double>(horizontal ? cellHeight : cellWidth) * scale;
        const int stride = horizontal ? 1 : columns();
        const int acrossStride = horizontal ? columns() : 1;
        const bool forward = to > from;
        const double edge = forward ? half : -half;
        // Sweep the leading edge. Exclude tiles that only touch the box.
        const int first = std::max(0, static_cast<int>(std::floor((std::min(from, to) + edge) / cell)));
        const int end = std::min(horizontal ? columns() : rows(),
                                 static_cast<int>(std::ceil((std::max(from, to) + edge) / cell)));
        const int acrossFirst = std::max(0, static_cast<int>(std::floor(acrossMin / acrossCell)));
        const int acrossEnd = std::min(horizontal ? rows() : columns(),
                                       static_cast<int>(std::ceil(acrossMax / acrossCell)));
        for (int along = forward ? first : end - 1; along >= first && along < end; along += forward ? 1 : -1) {
            for (int across = acrossFirst; across < acrossEnd; ++across) {
                if (blockedTiles[along * stride + across * acrossStride]) {
                    return forward ? along * cell - half : (along + 1) * cell + half;
                }
            }
        }
        return to;
    };

    const double x = local(from.x, position.x, halfSize.x, width);
    const double y = local(from.y, position.y, halfSize.y, height);
    const double nextX = clipAxis(x, targetX, halfSize.x, y - halfSize.y, y + halfSize.y, true);
    const double nextY = clipAxis(y, targetY, halfSize.y, nextX - halfSize.x, nextX + halfSize.x, false);
    return {static_cast<float>(position.x + nextX), static_cast<float>(position.y + nextY)};
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
    blockedTiles.clear();
    lastError.clear();
}
