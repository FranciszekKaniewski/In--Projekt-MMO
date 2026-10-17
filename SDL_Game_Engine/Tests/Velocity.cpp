#include "../Engine/ECS/ECS.h"
#include "../Engine/ECS/Components/TransformComponent.h"
#include "../Engine/ECS/Components/VelocityComponent.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void expectVector(math::Vector2 actual, math::Vector2 expected, const char* message) {
    constexpr float tolerance = 0.00001f;
    require(std::abs(actual.x - expected.x) <= tolerance &&
            std::abs(actual.y - expected.y) <= tolerance, message);
}

void testEmptyAndMultipleSources() {
    VelocityComponent velocity;
    expectVector(velocity.total(), {}, "An empty component should have zero velocity.");

    velocity.vectors["movement"] = {120.0f, 0.0f};
    velocity.vectors["wind"] = {0.0f, 30.0f};
    velocity.vectors["knockback"] = {-40.0f, 0.0f};
    const auto& readOnly = velocity;
    expectVector(readOnly.total(), {80.0f, 30.0f}, "Velocity sources were not summed.");
    require(velocity.vectors.size() == 3, "Summing changed the stored sources.");
    expectVector(velocity.vectors.at("movement"), {120.0f, 0.0f}, "Summing changed a source.");
}

void testReplacingOneSource() {
    VelocityComponent velocity;
    velocity.vectors["movement"] = {120.0f, 0.0f};
    velocity.vectors["wind"] = {0.0f, 30.0f};
    velocity.vectors["movement"] = {60.0f, 0.0f};

    require(velocity.vectors.size() == 2, "Updating a source created a duplicate.");
    expectVector(velocity.total(), {60.0f, 30.0f}, "Updating movement changed another source.");
}

void testRemovingAndClearingSources() {
    VelocityComponent velocity;
    velocity.vectors["movement"] = {120.0f, 0.0f};
    velocity.vectors["knockback"] = {-40.0f, 0.0f};

    velocity.vectors.erase("knockback");
    expectVector(velocity.total(), {120.0f, 0.0f}, "Removing knockback changed movement.");
    require(velocity.vectors.erase("missing") == 0, "Removing a missing source changed the component.");
    velocity.vectors.clear();
    expectVector(velocity.total(), {}, "Clearing sources did not stop movement.");
}

void testOppositeSourcesCancel() {
    VelocityComponent velocity;
    velocity.vectors["first"] = {90.0f, -30.0f};
    velocity.vectors["second"] = {-90.0f, 30.0f};
    expectVector(velocity.total(), {}, "Opposite velocities should cancel each other.");
}

void testMovementInECS() {
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

void runTest(const char* name, void (*test)(), int& failures) {
    try {
        test();
        std::cout << "PASS: " << name << '\n';
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << "FAIL: " << name << " - " << error.what() << '\n';
    }
}

} // namespace

int main() {
    int failures = 0;
    runTest("Empty component and multiple sources", testEmptyAndMultipleSources, failures);
    runTest("Replacing one source", testReplacingOneSource, failures);
    runTest("Removing and clearing sources", testRemovingAndClearingSources, failures);
    runTest("Opposite sources cancel", testOppositeSourcesCancel, failures);
    runTest("Movement in ECS", testMovementInECS, failures);
    std::cout << "\nFailed tests: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
