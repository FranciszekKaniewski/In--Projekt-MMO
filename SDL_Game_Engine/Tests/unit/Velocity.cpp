#include <catch_amalgamated.hpp>
#include "../../Engine/ECS/ECS.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/ECS/Components/VelocityComponent.h"

namespace {

void expectVector(math::Vector2 actual, math::Vector2 expected, const char* message) {
    constexpr float tolerance = 0.00001f;
    INFO(message);
    REQUIRE(actual.x == Catch::Approx(expected.x).epsilon(0).margin(tolerance));
    REQUIRE(actual.y == Catch::Approx(expected.y).epsilon(0).margin(tolerance));
}

TEST_CASE("Empty component and multiple sources", "[unit][velocity]") {
    VelocityComponent velocity;
    expectVector(velocity.total(), {}, "An empty component should have zero velocity.");

    velocity.vectors["movement"] = {120.0f, 0.0f};
    velocity.vectors["wind"] = {0.0f, 30.0f};
    velocity.vectors["knockback"] = {-40.0f, 0.0f};
    const auto& readOnly = velocity;
    expectVector(readOnly.total(), {80.0f, 30.0f}, "Velocity sources were not summed.");
    {
        INFO("Summing changed the stored sources.");
        REQUIRE(velocity.vectors.size() == 3);
    }
    expectVector(velocity.vectors.at("movement"), {120.0f, 0.0f}, "Summing changed a source.");
}

TEST_CASE("Replacing one source", "[unit][velocity]") {
    VelocityComponent velocity;
    velocity.vectors["movement"] = {120.0f, 0.0f};
    velocity.vectors["wind"] = {0.0f, 30.0f};
    velocity.vectors["movement"] = {60.0f, 0.0f};

    {
        INFO("Updating a source created a duplicate.");
        REQUIRE(velocity.vectors.size() == 2);
    }
    expectVector(velocity.total(), {60.0f, 30.0f}, "Updating movement changed another source.");
}

TEST_CASE("Removing and clearing sources", "[unit][velocity]") {
    VelocityComponent velocity;
    velocity.vectors["movement"] = {120.0f, 0.0f};
    velocity.vectors["knockback"] = {-40.0f, 0.0f};

    velocity.vectors.erase("knockback");
    expectVector(velocity.total(), {120.0f, 0.0f}, "Removing knockback changed movement.");
    {
        INFO("Removing a missing source changed the component.");
        REQUIRE(velocity.vectors.erase("missing") == 0);
    }
    velocity.vectors.clear();
    expectVector(velocity.total(), {}, "Clearing sources did not stop movement.");
}

TEST_CASE("Opposite sources cancel", "[unit][velocity]") {
    VelocityComponent velocity;
    velocity.vectors["first"] = {90.0f, -30.0f};
    velocity.vectors["second"] = {-90.0f, 30.0f};
    expectVector(velocity.total(), {}, "Opposite velocities should cancel each other.");
}

TEST_CASE("Movement in ECS", "[unit][velocity]") {
    ecs::Registry world;
    const auto moving = world.createEntity();
    world.addComponent<TransformComponent>(moving, math::Vector2{100.0f, 200.0f});
    auto& velocity = world.addComponent<VelocityComponent>(moving);
    velocity.vectors["movement"] = {120.0f, 0.0f};
    velocity.vectors["wind"] = {0.0f, 60.0f};
    velocity.vectors["knockback"] = {-60.0f, 0.0f};

    const auto stationary = world.createEntity();
    world.addComponent<TransformComponent>(stationary, math::Vector2{10.0f, 20.0f});

    world.forEach<TransformComponent, VelocityComponent>(
        [](ecs::Entity, TransformComponent& transform, const VelocityComponent& component) {
            transform.position += component.total() * 0.5f;
        });

    expectVector(world.getComponent<TransformComponent>(moving).position, {130.0f, 230.0f},
                 "Movement did not use total velocity and deltaTime.");
    expectVector(world.getComponent<TransformComponent>(stationary).position, {10.0f, 20.0f},
                 "Entity without velocity was moved.");
    expectVector(velocity.total(), {60.0f, 60.0f}, "Movement changed velocity sources.");
}

} // namespace
