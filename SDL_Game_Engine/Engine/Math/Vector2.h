#pragma once

#include <cmath>
#include <stdexcept>

namespace math {

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vector2() noexcept = default;
    constexpr Vector2(float x, float y) noexcept : x(x), y(y) {}

    constexpr Vector2& operator+=(const Vector2& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vector2& operator-=(const Vector2& other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vector2& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    Vector2& operator/=(float scalar) {
        if (scalar == 0.0f) {
            throw std::domain_error("Cannot divide a Vector2 by zero.");
        }
        x /= scalar;
        y /= scalar;
        return *this;
    }

    constexpr Vector2 operator-() const noexcept { return {-x, -y}; }

    constexpr float lengthSquared() const noexcept { return x * x + y * y; }

    float length() const noexcept { return std::hypot(x, y); }

    constexpr float dot(const Vector2& other) const noexcept {
        return x * other.x + y * other.y;
    }

    Vector2 normalized() const noexcept {
        const double magnitude = std::hypot(static_cast<double>(x), static_cast<double>(y));
        if (magnitude == 0.0) return {};
        return {static_cast<float>(x / magnitude), static_cast<float>(y / magnitude)};
    }

    Vector2& normalize() noexcept {
        *this = normalized();
        return *this;
    }
};

constexpr Vector2 operator+(Vector2 left, const Vector2& right) noexcept {
    return left += right;
}

constexpr Vector2 operator-(Vector2 left, const Vector2& right) noexcept {
    return left -= right;
}

constexpr Vector2 operator*(Vector2 vector, float scalar) noexcept {
    return vector *= scalar;
}

constexpr Vector2 operator*(float scalar, Vector2 vector) noexcept {
    return vector *= scalar;
}

inline Vector2 operator/(Vector2 vector, float scalar) {
    return vector /= scalar;
}

} // namespace math
