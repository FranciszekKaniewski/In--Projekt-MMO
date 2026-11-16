#include "ColliderDebugRenderer.h"

#include "../ECS/ECS.h"
#include "../ECS/Components/ColliderComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../ECS/Components/CameraComponent.h"
#include "../World/Map.h"
#include <SDL.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct ScopedDrawColor {
    SDL_Renderer* renderer;
    SDL_Color previous{};

    explicit ScopedDrawColor(SDL_Renderer* renderer) : renderer(renderer) {
        SDL_GetRenderDrawColor(renderer, &previous.r, &previous.g, &previous.b, &previous.a);
        SDL_SetRenderDrawColor(renderer, 64, 255, 128, 255);
    }

    ~ScopedDrawColor() {
        SDL_SetRenderDrawColor(renderer, previous.r, previous.g, previous.b, previous.a);
    }
};

struct ScopedDrawScale {
    SDL_Renderer* renderer;
    math::Vector2 scale{1.0f, 1.0f};

    explicit ScopedDrawScale(SDL_Renderer* renderer) : renderer(renderer) {
        SDL_RenderGetScale(renderer, &scale.x, &scale.y);
        // Project coordinates ourselves to keep outlines one physical pixel wide.
        // SDL's scaled point-based lines can disappear when the scale is below 1.
        // The viewport stays in place, including the letterbox offset.
        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    }

    ~ScopedDrawScale() {
        SDL_RenderSetScale(renderer, scale.x, scale.y);
    }
};

math::Vector2 project(math::Vector2 point, math::Vector2 scale) {
    return {point.x * scale.x, point.y * scale.y};
}

void drawCircle(SDL_Renderer* renderer, math::Vector2 center, float radius, math::Vector2 scale) {
    constexpr int segments = 64;

    static const auto unitCircle = [] {
        std::array<SDL_FPoint, segments> points{};
        constexpr float tau = 6.2831853071795864769f;
        for (int i = 0; i < segments; ++i) {
            const float angle = tau * i / segments;
            points[i] = {std::cos(angle), std::sin(angle)};
        }
        return points;
    }();

    std::array<SDL_FPoint, segments + 1> points{};
    for (int i = 0; i < segments; ++i) {
        points[i] = {(center.x + unitCircle[i].x * radius) * scale.x,
                     (center.y + unitCircle[i].y * radius) * scale.y};
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y)) return;
    }
    points.back() = points.front();
    SDL_RenderDrawLinesF(renderer, points.data(), static_cast<int>(points.size()));
}

void drawPolygon(SDL_Renderer* renderer, math::Vector2 origin,
                 const std::vector<math::Vector2>& vertices, math::Vector2 scale) {
    for (const auto& vertex : vertices) {
        const auto point = project(origin + vertex, scale);
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return;
    }

    auto previous = project(origin + vertices.back(), scale);
    for (const auto& vertex : vertices) {
        const auto current = project(origin + vertex, scale);
        SDL_RenderDrawLineF(renderer, previous.x, previous.y, current.x, current.y);
        previous = current;
    }
}

} // namespace

void renderMapColliderDebug(const Map& map, SDL_Renderer* renderer, bool devMode,
                            const CameraComponent* camera) {
    const auto& blocked = map.collisionTiles();
    if (!devMode || !renderer || !map.isLoaded() || blocked.empty()) return;

    const auto size = map.worldSize();
    const SDL_FRect bounds{map.position.x, map.position.y, size.x, size.y};
    if (!std::isfinite(bounds.x + bounds.w) || !std::isfinite(bounds.y + bounds.h) ||
        !(map.scale > 0.0f) || (camera && !camera->isVisible(bounds))) return;

    const auto tileSize = math::Vector2{map.tileWidth() * map.scale, map.tileHeight() * map.scale};
    const auto origin = camera ? map.position - camera->position : map.position;
    const int columns = map.columns();
    int firstColumn = 0, firstRow = 0, endColumn = columns, endRow = map.rows();
    if (camera) {
        // Restrict the grid to cells intersecting the camera, including partially visible cells.
        firstColumn = static_cast<int>(std::floor(std::clamp(-origin.x / tileSize.x, 0.0f, float(endColumn))));
        firstRow = static_cast<int>(std::floor(std::clamp(-origin.y / tileSize.y, 0.0f, float(endRow))));
        endColumn = static_cast<int>(std::ceil(std::clamp((camera->size.x - origin.x) / tileSize.x,
                                                          0.0f, float(endColumn))));
        endRow = static_cast<int>(std::ceil(std::clamp((camera->size.y - origin.y) / tileSize.y,
                                                       0.0f, float(endRow))));
    }

    const ScopedDrawColor drawColor(renderer);
    const ScopedDrawScale drawScale(renderer);
    const auto tilePixels = project(tileSize, drawScale.scale);
    std::vector<SDL_FRect> outlines;
    for (int row = firstRow; row < endRow; ++row) {
        for (int column = firstColumn; column < endColumn; ++column) {
            if (!blocked[row * columns + column]) continue;
            const auto corner = project(origin + math::Vector2{column * tileSize.x, row * tileSize.y},
                                        drawScale.scale);
            outlines.push_back({corner.x, corner.y, tilePixels.x, tilePixels.y});
        }
    }
    if (!outlines.empty()) SDL_RenderDrawRectsF(renderer, outlines.data(), static_cast<int>(outlines.size()));
}

void renderColliderDebug(ecs::Registry& world, SDL_Renderer* renderer, bool devMode,
                         const CameraComponent* camera) {
    if (!devMode || !renderer || (camera && !camera->isValid())) return;
    const ScopedDrawColor drawColor(renderer);
    const ScopedDrawScale drawScale(renderer);

    world.forEach<TransformComponent, ColliderComponent>(
        [renderer, camera, scale = drawScale.scale](ecs::Entity, const TransformComponent& transform,
                                            const ColliderComponent& collider) {
            if (!collider.enabled || !collider.isValid()) return;
            auto origin = transform.position + collider.offset;
            if (!std::isfinite(origin.x) || !std::isfinite(origin.y)) return;

            if (camera) {
                SDL_FRect bounds{};
                if (collider.shape == ColliderComponent::Shape::Circle) {
                    bounds = {origin.x - collider.radius, origin.y - collider.radius,
                              collider.radius * 2.0f, collider.radius * 2.0f};
                } else if (collider.shape == ColliderComponent::Shape::Box) {
                    bounds = {origin.x - collider.size.x * 0.5f, origin.y - collider.size.y * 0.5f,
                              collider.size.x, collider.size.y};
                } else {
                    auto minimum = collider.vertices.front();
                    auto maximum = minimum;
                    for (const auto& vertex : collider.vertices) {
                        minimum.x = std::min(minimum.x, vertex.x);
                        minimum.y = std::min(minimum.y, vertex.y);
                        maximum.x = std::max(maximum.x, vertex.x);
                        maximum.y = std::max(maximum.y, vertex.y);
                    }
                    bounds = {origin.x + minimum.x, origin.y + minimum.y,
                              maximum.x - minimum.x, maximum.y - minimum.y};
                }
                if (!camera->isVisible(bounds)) return;
                origin -= camera->position;
            }

            if (collider.shape == ColliderComponent::Shape::Circle) {
                drawCircle(renderer, origin, collider.radius, scale);
            } else if (collider.shape == ColliderComponent::Shape::Polygon) {
                drawPolygon(renderer, origin, collider.vertices, scale);
            } else {
                const SDL_FRect box{(origin.x - collider.size.x * 0.5f) * scale.x,
                                    (origin.y - collider.size.y * 0.5f) * scale.y,
                                    collider.size.x * scale.x, collider.size.y * scale.y};
                if (std::isfinite(box.x) && std::isfinite(box.y) &&
                    std::isfinite(box.x + box.w) && std::isfinite(box.y + box.h)) {
                    SDL_RenderDrawRectF(renderer, &box);
                }
            }
        });
}
