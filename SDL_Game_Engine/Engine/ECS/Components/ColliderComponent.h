#pragma once

#include "../../Math/Vector2.h"

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

struct ColliderComponent {
    enum class Shape { Box, Circle, Polygon };

    Shape shape = Shape::Box;

    math::Vector2 offset{};
    math::Vector2 size{32.0f, 32.0f}; // Box width and height; equal values form a square.
    float radius = 16.0f; // Used only by Circle.
    std::vector<math::Vector2> vertices; // Polygon boundary, in the supplied order.
    bool enabled = true;

    static ColliderComponent box(math::Vector2 size, math::Vector2 offset = {}) {
        ColliderComponent collider;
        collider.size = size;
        collider.offset = offset;
        if (!collider.isValid()) {
            throw std::invalid_argument("Box size must be positive and finite; offset must be finite.");
        }
        return collider;
    }

    static ColliderComponent square(float side, math::Vector2 offset = {}) {
        return box({side, side}, offset);
    }

    static ColliderComponent circle(float radius, math::Vector2 offset = {}) {
        ColliderComponent collider;
        collider.shape = Shape::Circle;
        collider.radius = radius;
        collider.offset = offset;
        if (!collider.isValid()) {
            throw std::invalid_argument("Circle radius must be positive and finite; offset must be finite.");
        }
        return collider;
    }

    static ColliderComponent polygon(std::vector<math::Vector2> vertices, math::Vector2 offset = {}) {
        ColliderComponent collider;
        collider.shape = Shape::Polygon;
        collider.vertices = std::move(vertices);
        collider.offset = offset;
        if (!collider.isValid()) {
            throw std::invalid_argument("Polygon needs at least three finite vertices and a finite offset.");
        }
        return collider;
    }

    bool isValid() const noexcept {
        if (!std::isfinite(offset.x) || !std::isfinite(offset.y)) return false;
        switch (shape) {
            case Shape::Box:
                return size.x > 0 && size.y > 0 && std::isfinite(size.x) && std::isfinite(size.y);
            case Shape::Circle:
                return radius > 0 && std::isfinite(radius);
            case Shape::Polygon:
                if (vertices.size() < 3) return false;
                for (const auto& vertex : vertices) {
                    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) return false;
                }
                return true;
        }
        return false;
    }
};