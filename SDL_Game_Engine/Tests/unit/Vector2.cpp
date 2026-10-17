#include <catch_amalgamated.hpp>
#include "../../Engine/Math/Vector2.h"

#include <limits>
#include <stdexcept>

using math::Vector2;

namespace {

// Wyniki float porownujemy z tolerancja zamiast przez operator ==.
void expectNear(float actual, float expected, const char* message) {
    constexpr float tolerance = 0.00001f;
    INFO(message);
    REQUIRE(actual == Catch::Approx(expected).epsilon(0).margin(tolerance));
}

void expectVector(Vector2 actual, Vector2 expected, const char* message) {
    expectNear(actual.x, expected.x, message);
    expectNear(actual.y, expected.y, message);
}

TEST_CASE("Construction", "[unit][vector2]") {
    expectVector(Vector2{}, {0.0f, 0.0f}, "Default vector should be zero.");
    expectVector(Vector2{2.0f, -3.0f}, {2.0f, -3.0f}, "Coordinates should be preserved.");
}

TEST_CASE("Arithmetic preserves inputs", "[unit][vector2]") {
    Vector2 a{6.0f, 8.0f};
    const Vector2 b{2.0f, 4.0f};

    expectVector(a + b, {8.0f, 12.0f}, "Addition failed.");
    expectVector(a - b, {4.0f, 4.0f}, "Subtraction failed.");
    expectVector(a * 0.5f, {3.0f, 4.0f}, "Fractional multiplication failed.");
    expectVector(0.5f * a, {3.0f, 4.0f}, "Scalar on the left failed.");
    expectVector(a / 2.0f, {3.0f, 4.0f}, "Division failed.");
    expectVector(-a, {-6.0f, -8.0f}, "Negation failed.");
    expectVector(a, {6.0f, 8.0f}, "Arithmetic changed the left input.");
    expectVector(b, {2.0f, 4.0f}, "Arithmetic changed the right input.");
}

TEST_CASE("Compound assignments", "[unit][vector2]") {
    Vector2 value{2.0f, 4.0f};

    value += Vector2{2.0f, 2.0f};
    expectVector(value, {4.0f, 6.0f}, "+= failed.");
    value -= Vector2{1.0f, 2.0f};
    expectVector(value, {3.0f, 4.0f}, "-= failed.");
    value *= 2.0f;
    expectVector(value, {6.0f, 8.0f}, "*= failed.");
    value /= 2.0f;
    expectVector(value, {3.0f, 4.0f}, "/= failed.");
}

TEST_CASE("Length and dot product", "[unit][vector2]") {
    const Vector2 value{3.0f, 4.0f};

    expectNear(value.length(), 5.0f, "Length failed.");
    expectNear(value.lengthSquared(), 25.0f, "Squared length failed.");
    expectNear(value.dot({2.0f, -1.0f}), 2.0f, "Dot product failed.");
    expectNear(Vector2{1.0f, 0.0f}.dot({0.0f, 1.0f}), 0.0f,
               "Perpendicular vectors should have a zero dot product.");
}

TEST_CASE("Normalization", "[unit][vector2]") {
    Vector2 value{3.0f, 4.0f};

    const Vector2 unit = value.normalized();
    expectVector(unit, {0.6f, 0.8f}, "normalized() returned a wrong direction.");
    expectNear(unit.length(), 1.0f, "Normalized length should be one.");
    expectVector(value, {3.0f, 4.0f}, "normalized() changed its input.");

    value.normalize();
    expectVector(value, {0.6f, 0.8f}, "normalize() did not modify its input.");
}

TEST_CASE("Zero normalization", "[unit][vector2]") {
    Vector2 zero;

    expectVector(zero.normalized(), {}, "Normalizing zero should return zero.");
    zero.normalize();
    expectVector(zero, {}, "In-place normalization of zero should leave zero.");
}

TEST_CASE("Extreme normalization", "[unit][vector2]") {
    const float largest = std::numeric_limits<float>::max();
    const Vector2 large = Vector2{largest, largest}.normalized();
    expectNear(large.x, 0.70710678f, "Large vector normalization failed.");
    expectNear(large.y, 0.70710678f, "Large vector normalization failed.");

    const float smallest = std::numeric_limits<float>::denorm_min();
    expectVector(Vector2{smallest, 0.0f}.normalized(), {1.0f, 0.0f},
                 "Small vector normalization failed.");
}

TEST_CASE("Division by zero", "[unit][vector2]") {
    Vector2 value{8.0f, 4.0f};

    for (float zero : {0.0f, -0.0f}) {
        REQUIRE_THROWS_AS(value / zero, std::domain_error);
        REQUIRE_THROWS_AS(value /= zero, std::domain_error);
        expectVector(value, {8.0f, 4.0f}, "Rejected division changed the vector.");
    }
}

TEST_CASE("Movement with deltaTime", "[unit][vector2]") {
    Vector2 position{100.0f, 200.0f};
    const Vector2 velocity{60.0f, -30.0f};
    const float deltaTime = 1.0f / 60.0f;

    position += velocity * deltaTime;
    expectVector(position, {101.0f, 199.5f}, "Movement after one step failed.");

    for (int step = 1; step < 60; ++step) {
        position += velocity * deltaTime;
    }
    expectVector(position, {160.0f, 170.0f}, "Movement after one second failed.");
    expectVector(velocity, {60.0f, -30.0f}, "Movement changed velocity.");
}

} // namespace
