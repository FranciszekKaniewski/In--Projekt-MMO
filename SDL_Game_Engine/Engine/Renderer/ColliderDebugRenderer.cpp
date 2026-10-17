#include "ColliderDebugRenderer.h"

#include "../ECS/ECS.h"
#include "../ECS/Components/ColliderComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include <SDL.h>

#include <array>
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

void drawCircle(SDL_Renderer* renderer, math::Vector2 center, float radius) {
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
        points[i] = {center.x + unitCircle[i].x * radius, center.y + unitCircle[i].y * radius};
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y)) return;
    }
    points.back() = points.front();
    SDL_RenderDrawLinesF(renderer, points.data(), static_cast<int>(points.size()));
}

void drawPolygon(SDL_Renderer* renderer, math::Vector2 origin,
                 const std::vector<math::Vector2>& vertices) {
    for (const auto& vertex : vertices) {
        const auto point = origin + vertex;
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return;
    }

    auto previous = origin + vertices.back();
    for (const auto& vertex : vertices) {
        const auto current = origin + vertex;
        SDL_RenderDrawLineF(renderer, previous.x, previous.y, current.x, current.y);
        previous = current;
    }
}

} // namespace

void renderColliderDebug(ecs::Registry& world, SDL_Renderer* renderer, bool devMode) {
    if (!devMode || !renderer) return;
    const ScopedDrawColor drawColor(renderer);

    world.forEach<TransformComponent, ColliderComponent>(
        [renderer](ecs::Entity, const TransformComponent& transform, const ColliderComponent& collider) {
            if (!collider.enabled || !collider.isValid()) return;
            const auto origin = transform.position + collider.offset;
            if (!std::isfinite(origin.x) || !std::isfinite(origin.y)) return;

            if (collider.shape == ColliderComponent::Shape::Circle) {
                drawCircle(renderer, origin, collider.radius);
            } else if (collider.shape == ColliderComponent::Shape::Polygon) {
                drawPolygon(renderer, origin, collider.vertices);
            } else {
                const SDL_FRect box{origin.x - collider.size.x * 0.5f,
                                    origin.y - collider.size.y * 0.5f,
                                    collider.size.x, collider.size.y};
                if (std::isfinite(box.x) && std::isfinite(box.y) &&
                    std::isfinite(box.x + box.w) && std::isfinite(box.y + box.h)) {
                    SDL_RenderDrawRectF(renderer, &box);
                }
            }
        });
}