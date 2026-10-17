#include <catch_amalgamated.hpp>
#define SDL_MAIN_HANDLED
#include "../../Engine/ECS/ECS.h"
#include "../../Engine/ECS/Components/ColliderComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/Renderer/ColliderDebugRenderer.h"
#include <SDL.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

struct SDLFixture {
    SDLFixture() {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            const std::string error = SDL_GetError();
            SDL_Quit();
            FAIL("SDL initialization: " << error);
        }
    }
    ~SDLFixture() { SDL_Quit(); }
};

// Software rendering on an in-memory surface: no window or game assets needed.
class Canvas {
    static constexpr int width = 128;
    static constexpr int height = 128;
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface{nullptr, SDL_FreeSurface};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer{nullptr, SDL_DestroyRenderer};

public:
    Canvas() {
        surface.reset(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32));
        {
            INFO("Could not create test surface.");
            REQUIRE(surface != nullptr);
        }
        renderer.reset(SDL_CreateSoftwareRenderer(surface.get()));
        {
            INFO("Could not create software renderer.");
            REQUIRE(renderer != nullptr);
        }
        clear();
    }

    SDL_Renderer* get() const { return renderer.get(); }

    void clear() {
        SDL_SetRenderDrawColor(get(), 0, 0, 0, 255);
        {
            INFO("Could not clear test surface.");
            REQUIRE(SDL_RenderClear(get()) == 0);
        }
    }

    bool isOutlineAt(int x, int y) const {
        const SDL_Rect area{x, y, 1, 1};
        std::array<Uint8, 4> pixel{};
        {
            INFO("Could not read test pixel.");
            REQUIRE(SDL_RenderReadPixels(get(), &area, SDL_PIXELFORMAT_RGBA32, pixel.data(), 4) == 0);
        }
        return pixel[0] == 64 && pixel[1] == 255 && pixel[2] == 128;
    }

    int outlinePixels() const {
        std::array<Uint8, width * height * 4> pixels{};
        {
            INFO("Could not read test surface.");
            REQUIRE(SDL_RenderReadPixels(get(), nullptr, SDL_PIXELFORMAT_RGBA32,
                                     pixels.data(), width * 4) == 0);
        }
        int count = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 4) {
            if (pixels[i] == 64 && pixels[i + 1] == 255 && pixels[i + 2] == 128) ++count;
        }
        return count;
    }
};

ecs::Entity addCollider(ecs::Registry& world, math::Vector2 position, ColliderComponent collider) {
    const auto entity = world.createEntity();
    world.addComponent<TransformComponent>(entity, position);
    world.addComponent<ColliderComponent>(entity, collider);
    return entity;
}

TEST_CASE_METHOD(SDLFixture, "Developer mode and shape outlines", "[integration][collider]") {
    ecs::Registry world;
    Canvas canvas;
    addCollider(world, {32.0f, 32.0f}, ColliderComponent::square(20.0f));
    addCollider(world, {85.0f, 35.0f}, ColliderComponent::circle(12.0f));
    addCollider(world, {70.0f, 90.0f}, ColliderComponent::polygon({
        {0.0f, -15.0f}, {15.0f, 15.0f}, {-15.0f, 15.0f}
    }));

    renderColliderDebug(world, canvas.get(), false);
    {
        INFO("Hitboxes are visible outside developer mode.");
        REQUIRE(canvas.outlinePixels() == 0);
    }

    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Square outline is misplaced.");
        REQUIRE((canvas.isOutlineAt(22, 32) && canvas.isOutlineAt(32, 22)));
    }
    {
        INFO("Circle outline is misplaced.");
        REQUIRE((canvas.isOutlineAt(97, 35) && canvas.isOutlineAt(85, 23)));
    }
    {
        INFO("Hitboxes should not be filled.");
        REQUIRE((!canvas.isOutlineAt(32, 32) && !canvas.isOutlineAt(85, 35)));
    }
    {
        INFO("Triangle outline is misplaced.");
        REQUIRE((canvas.isOutlineAt(70, 75) && canvas.isOutlineAt(70, 105)));
    }
    {
        INFO("Polygon should not be filled.");
        REQUIRE(!canvas.isOutlineAt(70, 90));
    }

    canvas.clear();
    renderColliderDebug(world, canvas.get(), false);
    {
        INFO("Hitboxes remain after disabling developer mode.");
        REQUIRE(canvas.outlinePixels() == 0);
    }
    renderColliderDebug(world, nullptr, true); // Safe when rendering is unavailable.
}

TEST_CASE_METHOD(SDLFixture, "Position and offset", "[integration][collider]") {
    ecs::Registry world;
    Canvas canvas;
    const auto square = addCollider(world, {20.0f, 20.0f},
                                    ColliderComponent::square(20.0f, {10.0f, 5.0f}));
    addCollider(world, {75.0f, 95.0f}, ColliderComponent::circle(8.0f, {5.0f, 0.0f}));
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Square offset was ignored.");
        REQUIRE(canvas.isOutlineAt(20, 25));
    }
    {
        INFO("Circle offset was ignored.");
        REQUIRE(canvas.isOutlineAt(88, 95));
    }

    world.getComponent<TransformComponent>(square).position = {70.0f, 60.0f};
    canvas.clear();
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Collider did not follow the entity.");
        REQUIRE(canvas.isOutlineAt(70, 65));
    }
    {
        INFO("Collider remained at its old position.");
        REQUIRE(!canvas.isOutlineAt(20, 25));
    }
}

TEST_CASE_METHOD(SDLFixture, "Disabled and missing components", "[integration][collider]") {
    ecs::Registry world;
    Canvas canvas;
    const auto disabled = addCollider(world, {32.0f, 32.0f}, ColliderComponent::square(20.0f));
    world.getComponent<ColliderComponent>(disabled).enabled = false;
    world.addComponent<TransformComponent>(world.createEntity(), math::Vector2{60.0f, 60.0f});
    world.addComponent<ColliderComponent>(world.createEntity(), ColliderComponent::circle(10.0f));
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Disabled or incomplete collider was drawn.");
        REQUIRE(canvas.outlinePixels() == 0);
    }
}

TEST_CASE_METHOD(SDLFixture, "Renderer draw color is preserved", "[integration][collider]") {
    ecs::Registry world;
    Canvas canvas;
    addCollider(world, {32.0f, 32.0f}, ColliderComponent::square(20.0f));
    SDL_SetRenderDrawColor(canvas.get(), 11, 22, 33, 44);
    renderColliderDebug(world, canvas.get(), true);
    SDL_Color color{};
    SDL_GetRenderDrawColor(canvas.get(), &color.r, &color.g, &color.b, &color.a);
    {
        INFO("Debug rendering changed the renderer draw color.");
        REQUIRE((color.r == 11 && color.g == 22 && color.b == 33 && color.a == 44));
    }
}

TEST_CASE_METHOD(SDLFixture, "Invalid collider data", "[integration][collider]") {
    for (float size : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()}) {
        CAPTURE(size);
        REQUIRE_THROWS_AS(ColliderComponent::square(size), std::invalid_argument);
        REQUIRE_THROWS_AS(ColliderComponent::circle(size), std::invalid_argument);
    }

    ecs::Registry world;
    Canvas canvas;
    const auto invalid = addCollider(world, {32.0f, 32.0f}, ColliderComponent::square(20.0f));
    world.getComponent<ColliderComponent>(invalid).size.x = -1.0f;
    addCollider(world, {std::numeric_limits<float>::infinity(), 32.0f}, ColliderComponent::circle(10.0f));
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Invalid collider data was passed to the renderer.");
        REQUIRE(canvas.outlinePixels() == 0);
    }
}

TEST_CASE_METHOD(SDLFixture, "Concave polygon and closing edge", "[integration][collider]") {
    ecs::Registry world;
    Canvas canvas;
    addCollider(world, {10.0f, 10.0f}, ColliderComponent::polygon({
        {0.0f, 0.0f}, {40.0f, 0.0f}, {40.0f, 15.0f},
        {15.0f, 15.0f}, {15.0f, 40.0f}, {0.0f, 40.0f}
    }));
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Polygon top edge is missing.");
        REQUIRE(canvas.isOutlineAt(30, 10));
    }
    {
        INFO("Concave edges do not follow the supplied vertex order.");
        REQUIRE((canvas.isOutlineAt(40, 25) && canvas.isOutlineAt(25, 40)));
    }
    {
        INFO("Last vertex was not connected to the first.");
        REQUIRE(canvas.isOutlineAt(10, 30));
    }
    {
        INFO("Polygon was filled.");
        REQUIRE((!canvas.isOutlineAt(15, 15) && !canvas.isOutlineAt(40, 40)));
    }
}

TEST_CASE_METHOD(SDLFixture, "Polygon position and offset", "[integration][collider]") {
    ecs::Registry world;
    Canvas canvas;
    const auto polygon = addCollider(world, {20.0f, 20.0f}, ColliderComponent::polygon({
        {0.0f, 0.0f}, {20.0f, 0.0f}, {0.0f, 20.0f}
    }, {10.0f, 5.0f}));
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Polygon position or offset was ignored.");
        REQUIRE(canvas.isOutlineAt(40, 25));
    }

    world.getComponent<TransformComponent>(polygon).position = {70.0f, 60.0f};
    canvas.clear();
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Polygon did not follow its entity.");
        REQUIRE((canvas.isOutlineAt(90, 65) && canvas.isOutlineAt(80, 75)));
    }
    {
        INFO("Polygon remained at its old position.");
        REQUIRE(!canvas.isOutlineAt(40, 25));
    }

    world.getComponent<ColliderComponent>(polygon).enabled = false;
    canvas.clear();
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Disabled polygon was drawn.");
        REQUIRE(canvas.outlinePixels() == 0);
    }

    auto& collider = world.getComponent<ColliderComponent>(polygon);
    collider.enabled = true;
    collider.vertices.pop_back();
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Polygon edited to have fewer than three vertices was drawn.");
        REQUIRE(canvas.outlinePixels() == 0);
    }
}

TEST_CASE_METHOD(SDLFixture, "Polygon validation", "[integration][collider]") {
    for (std::size_t count = 0; count < 3; ++count) {
        CAPTURE(count);
        REQUIRE_THROWS_AS(ColliderComponent::polygon(std::vector<math::Vector2>(count)),
                          std::invalid_argument);
    }
    for (const auto invalid : {math::Vector2{std::numeric_limits<float>::infinity(), 0.0f},
                               math::Vector2{0.0f, std::numeric_limits<float>::quiet_NaN()}}) {
        REQUIRE_THROWS_AS(ColliderComponent::polygon({{0.0f, 0.0f}, {10.0f, 0.0f}, invalid}),
                          std::invalid_argument);
    }

    REQUIRE_THROWS_AS(ColliderComponent::polygon({{0.0f, 0.0f}, {10.0f, 0.0f}, {0.0f, 10.0f}},
                                                {std::numeric_limits<float>::infinity(), 0.0f}),
                      std::invalid_argument);

    ecs::Registry world;
    Canvas canvas;
    const auto entity = addCollider(world, {32.0f, 32.0f}, ColliderComponent::polygon({
        {0.0f, 0.0f}, {20.0f, 0.0f}, {0.0f, 20.0f}
    }));
    world.getComponent<ColliderComponent>(entity).vertices.back().x = std::numeric_limits<float>::quiet_NaN();
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Invalid polygon was partially drawn.");
        REQUIRE(canvas.outlinePixels() == 0);
    }
}

TEST_CASE_METHOD(SDLFixture, "Many polygon vertices and ownership", "[integration][collider]") {
    constexpr int count = 128;
    std::vector<math::Vector2> vertices;
    for (int i = 0; i < count; ++i) {
        const float angle = 6.2831853071795864769f * i / count;
        vertices.push_back({std::cos(angle) * 20.0f, std::sin(angle) * 20.0f});
    }
    const auto polygon = ColliderComponent::polygon(vertices);
    vertices.clear();
    {
        INFO("Polygon did not retain its own vertices.");
        REQUIRE((polygon.vertices.size() == count && polygon.isValid()));
    }

    ecs::Registry world;
    Canvas canvas;
    addCollider(world, {64.0f, 64.0f}, polygon);
    renderColliderDebug(world, canvas.get(), true);
    {
        INFO("Large polygon was not drawn.");
        REQUIRE((canvas.isOutlineAt(84, 64) && canvas.isOutlineAt(64, 44)));
    }
    {
        INFO("Large polygon was filled.");
        REQUIRE(!canvas.isOutlineAt(64, 64));
    }
}

} // namespace
