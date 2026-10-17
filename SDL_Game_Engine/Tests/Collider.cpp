#define SDL_MAIN_HANDLED
#include "../Engine/ECS/ECS.h"
#include "../Engine/ECS/Components/ColliderComponent.h"
#include "../Engine/ECS/Components/TransformComponent.h"
#include "../Engine/Renderer/ColliderDebugRenderer.h"
#include "../Engine/Vendor/SDL2/include/SDL2/SDL.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Software rendering on an in-memory surface: no window or game assets needed.
class Canvas {
    static constexpr int width = 128;
    static constexpr int height = 128;
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface{nullptr, SDL_FreeSurface};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer{nullptr, SDL_DestroyRenderer};

public:
    Canvas() {
        surface.reset(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32));
        require(surface != nullptr, "Could not create test surface.");
        renderer.reset(SDL_CreateSoftwareRenderer(surface.get()));
        require(renderer != nullptr, "Could not create software renderer.");
        clear();
    }

    SDL_Renderer* get() const { return renderer.get(); }

    void clear() {
        SDL_SetRenderDrawColor(get(), 0, 0, 0, 255);
        require(SDL_RenderClear(get()) == 0, "Could not clear test surface.");
    }

    bool isOutlineAt(int x, int y) const {
        const SDL_Rect area{x, y, 1, 1};
        std::array<Uint8, 4> pixel{};
        require(SDL_RenderReadPixels(get(), &area, SDL_PIXELFORMAT_RGBA32, pixel.data(), 4) == 0,
                "Could not read test pixel.");
        return pixel[0] == 64 && pixel[1] == 255 && pixel[2] == 128;
    }

    int outlinePixels() const {
        std::array<Uint8, width * height * 4> pixels{};
        require(SDL_RenderReadPixels(get(), nullptr, SDL_PIXELFORMAT_RGBA32,
                                     pixels.data(), width * 4) == 0, "Could not read test surface.");
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

void testDeveloperModeAndOutlines() {
    ecs::Registry world;
    Canvas canvas;
    addCollider(world, {32.0f, 32.0f}, ColliderComponent::square(20.0f));
    addCollider(world, {85.0f, 35.0f}, ColliderComponent::circle(12.0f));
    addCollider(world, {70.0f, 90.0f}, ColliderComponent::polygon({
        {0.0f, -15.0f}, {15.0f, 15.0f}, {-15.0f, 15.0f}
    }));

    renderColliderDebug(world, canvas.get(), false);
    require(canvas.outlinePixels() == 0, "Hitboxes are visible outside developer mode.");

    renderColliderDebug(world, canvas.get(), true);
    require(canvas.isOutlineAt(22, 32) && canvas.isOutlineAt(32, 22), "Square outline is misplaced.");
    require(canvas.isOutlineAt(97, 35) && canvas.isOutlineAt(85, 23), "Circle outline is misplaced.");
    require(!canvas.isOutlineAt(32, 32) && !canvas.isOutlineAt(85, 35), "Hitboxes should not be filled.");
    require(canvas.isOutlineAt(70, 75) && canvas.isOutlineAt(70, 105), "Triangle outline is misplaced.");
    require(!canvas.isOutlineAt(70, 90), "Polygon should not be filled.");

    canvas.clear();
    renderColliderDebug(world, canvas.get(), false);
    require(canvas.outlinePixels() == 0, "Hitboxes remain after disabling developer mode.");
    renderColliderDebug(world, nullptr, true); // Safe when rendering is unavailable.
}

void testPositionAndOffset() {
    ecs::Registry world;
    Canvas canvas;
    const auto square = addCollider(world, {20.0f, 20.0f},
                                    ColliderComponent::square(20.0f, {10.0f, 5.0f}));
    addCollider(world, {75.0f, 95.0f}, ColliderComponent::circle(8.0f, {5.0f, 0.0f}));
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.isOutlineAt(20, 25), "Square offset was ignored.");
    require(canvas.isOutlineAt(88, 95), "Circle offset was ignored.");

    world.getComponent<TransformComponent>(square).position = {70.0f, 60.0f};
    canvas.clear();
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.isOutlineAt(70, 65), "Collider did not follow the entity.");
    require(!canvas.isOutlineAt(20, 25), "Collider remained at its old position.");
}

void testDisabledAndMissingComponents() {
    ecs::Registry world;
    Canvas canvas;
    const auto disabled = addCollider(world, {32.0f, 32.0f}, ColliderComponent::square(20.0f));
    world.getComponent<ColliderComponent>(disabled).enabled = false;
    world.addComponent<TransformComponent>(world.createEntity(), math::Vector2{60.0f, 60.0f});
    world.addComponent<ColliderComponent>(world.createEntity(), ColliderComponent::circle(10.0f));
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.outlinePixels() == 0, "Disabled or incomplete collider was drawn.");
}

void testDrawColorIsPreserved() {
    ecs::Registry world;
    Canvas canvas;
    addCollider(world, {32.0f, 32.0f}, ColliderComponent::square(20.0f));
    SDL_SetRenderDrawColor(canvas.get(), 11, 22, 33, 44);
    renderColliderDebug(world, canvas.get(), true);
    SDL_Color color{};
    SDL_GetRenderDrawColor(canvas.get(), &color.r, &color.g, &color.b, &color.a);
    require(color.r == 11 && color.g == 22 && color.b == 33 && color.a == 44,
            "Debug rendering changed the renderer draw color.");
}

void testInvalidDimensions() {
    for (float size : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()}) {
        bool rejected = false;
        try { ColliderComponent::square(size); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid square size was accepted.");

        rejected = false;
        try { ColliderComponent::circle(size); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid circle radius was accepted.");
    }

    ecs::Registry world;
    Canvas canvas;
    const auto invalid = addCollider(world, {32.0f, 32.0f}, ColliderComponent::square(20.0f));
    world.getComponent<ColliderComponent>(invalid).size.x = -1.0f;
    addCollider(world, {std::numeric_limits<float>::infinity(), 32.0f}, ColliderComponent::circle(10.0f));
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.outlinePixels() == 0, "Invalid collider data was passed to the renderer.");
}

void testConcavePolygonAndClosingEdge() {
    ecs::Registry world;
    Canvas canvas;
    addCollider(world, {10.0f, 10.0f}, ColliderComponent::polygon({
        {0.0f, 0.0f}, {40.0f, 0.0f}, {40.0f, 15.0f},
        {15.0f, 15.0f}, {15.0f, 40.0f}, {0.0f, 40.0f}
    }));
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.isOutlineAt(30, 10), "Polygon top edge is missing.");
    require(canvas.isOutlineAt(40, 25) && canvas.isOutlineAt(25, 40),
            "Concave edges do not follow the supplied vertex order.");
    require(canvas.isOutlineAt(10, 30), "Last vertex was not connected to the first.");
    require(!canvas.isOutlineAt(15, 15) && !canvas.isOutlineAt(40, 40), "Polygon was filled.");
}

void testPolygonPositionAndOffset() {
    ecs::Registry world;
    Canvas canvas;
    const auto polygon = addCollider(world, {20.0f, 20.0f}, ColliderComponent::polygon({
        {0.0f, 0.0f}, {20.0f, 0.0f}, {0.0f, 20.0f}
    }, {10.0f, 5.0f}));
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.isOutlineAt(40, 25), "Polygon position or offset was ignored.");

    world.getComponent<TransformComponent>(polygon).position = {70.0f, 60.0f};
    canvas.clear();
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.isOutlineAt(90, 65) && canvas.isOutlineAt(80, 75), "Polygon did not follow its entity.");
    require(!canvas.isOutlineAt(40, 25), "Polygon remained at its old position.");

    world.getComponent<ColliderComponent>(polygon).enabled = false;
    canvas.clear();
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.outlinePixels() == 0, "Disabled polygon was drawn.");

    auto& collider = world.getComponent<ColliderComponent>(polygon);
    collider.enabled = true;
    collider.vertices.pop_back();
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.outlinePixels() == 0, "Polygon edited to have fewer than three vertices was drawn.");
}

void testPolygonValidation() {
    for (std::size_t count = 0; count < 3; ++count) {
        bool rejected = false;
        try { ColliderComponent::polygon(std::vector<math::Vector2>(count)); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Polygon with fewer than three vertices was accepted.");
    }
    for (const auto invalid : {math::Vector2{std::numeric_limits<float>::infinity(), 0.0f},
                               math::Vector2{0.0f, std::numeric_limits<float>::quiet_NaN()}}) {
        bool rejected = false;
        try { ColliderComponent::polygon({{0.0f, 0.0f}, {10.0f, 0.0f}, invalid}); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Non-finite polygon vertex was accepted.");
    }

    bool rejected = false;
    try {
        ColliderComponent::polygon({{0.0f, 0.0f}, {10.0f, 0.0f}, {0.0f, 10.0f}},
                                   {std::numeric_limits<float>::infinity(), 0.0f});
    } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Non-finite polygon offset was accepted.");

    ecs::Registry world;
    Canvas canvas;
    const auto entity = addCollider(world, {32.0f, 32.0f}, ColliderComponent::polygon({
        {0.0f, 0.0f}, {20.0f, 0.0f}, {0.0f, 20.0f}
    }));
    world.getComponent<ColliderComponent>(entity).vertices.back().x = std::numeric_limits<float>::quiet_NaN();
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.outlinePixels() == 0, "Invalid polygon was partially drawn.");
}

void testManyPolygonVerticesAndOwnership() {
    constexpr int count = 128;
    std::vector<math::Vector2> vertices;
    for (int i = 0; i < count; ++i) {
        const float angle = 6.2831853071795864769f * i / count;
        vertices.push_back({std::cos(angle) * 20.0f, std::sin(angle) * 20.0f});
    }
    const auto polygon = ColliderComponent::polygon(vertices);
    vertices.clear();
    require(polygon.vertices.size() == count && polygon.isValid(), "Polygon did not retain its own vertices.");

    ecs::Registry world;
    Canvas canvas;
    addCollider(world, {64.0f, 64.0f}, polygon);
    renderColliderDebug(world, canvas.get(), true);
    require(canvas.isOutlineAt(84, 64) && canvas.isOutlineAt(64, 44), "Large polygon was not drawn.");
    require(!canvas.isOutlineAt(64, 64), "Large polygon was filled.");
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
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "FAIL: SDL initialization - " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    int failures = 0;
    runTest("Developer mode and shape outlines", testDeveloperModeAndOutlines, failures);
    runTest("Position and offset", testPositionAndOffset, failures);
    runTest("Disabled and missing components", testDisabledAndMissingComponents, failures);
    runTest("Renderer draw color is preserved", testDrawColorIsPreserved, failures);
    runTest("Invalid collider data", testInvalidDimensions, failures);
    runTest("Concave polygon and closing edge", testConcavePolygonAndClosingEdge, failures);
    runTest("Polygon position and offset", testPolygonPositionAndOffset, failures);
    runTest("Polygon validation", testPolygonValidation, failures);
    runTest("Many polygon vertices and ownership", testManyPolygonVerticesAndOwnership, failures);
    SDL_Quit();
    std::cout << "\nFailed tests: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
