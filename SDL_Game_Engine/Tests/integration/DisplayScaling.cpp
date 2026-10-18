#include <catch_amalgamated.hpp>
#define SDL_MAIN_HANDLED
#include "../../Engine/Core/App.h"
#include "../../Engine/ECS/ECS.h"
#include "../../Engine/ECS/Components/ColliderComponent.h"
#include "../../Engine/ECS/Components/SpriteComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/Renderer/ColliderDebugRenderer.h"
#include "../../Engine/Renderer/SpriteRenderer.h"

#include <cmath>
#include <cstring>
#include <memory>

namespace {

constexpr SDL_Color white{255, 255, 255, 255};
constexpr SDL_Color outline{64, 255, 128, 255};
constexpr SDL_Color black{0, 0, 0, 255};

struct DisplaySDLFixture {
    DisplaySDLFixture() {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
    }
    ~DisplaySDLFixture() { SDL_Quit(); }
};

// Read physical pixels directly from the backing surface, outside SDL's viewport mapping.
class DisplayCanvas {
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface{nullptr, SDL_FreeSurface};

public:
    App app; // Destroy its renderer before freeing the backing surface.

    DisplayCanvas(int width, int height) {
        REQUIRE(App::renderer == nullptr);
        app.window = SDL_CreateWindow("Scaling test", 0, 0, width, height, SDL_WINDOW_HIDDEN);
        REQUIRE(app.window != nullptr);
        surface.reset(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32));
        REQUIRE(surface != nullptr);
        App::renderer = SDL_CreateSoftwareRenderer(surface.get());
        REQUIRE(App::renderer != nullptr);
        app.syncDisplaySettings();
        SDL_SetRenderDrawColor(App::renderer, 0, 0, 0, 255);
        REQUIRE(SDL_RenderClear(App::renderer) == 0);
    }

    bool isColorAt(int x, int y, SDL_Color expected) const {
        REQUIRE(SDL_RenderFlush(App::renderer) == 0);
        REQUIRE((x >= 0 && x < surface->w && y >= 0 && y < surface->h));
        Uint32 pixel = 0;
        const auto* address = static_cast<const Uint8*>(surface->pixels) + y * surface->pitch + x * 4;
        std::memcpy(&pixel, address, sizeof(pixel));
        SDL_Color actual{};
        SDL_GetRGBA(pixel, surface->format, &actual.r, &actual.g, &actual.b, &actual.a);
        return actual.r == expected.r && actual.g == expected.g &&
               actual.b == expected.b && actual.a == expected.a;
    }

    TexturePtr whiteTexture() const {
        TexturePtr texture{SDL_CreateTexture(App::renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, 1, 1), SDL_DestroyTexture};
        REQUIRE(texture != nullptr);
        const Uint8 pixel[]{255, 255, 255, 255};
        REQUIRE(SDL_UpdateTexture(texture.get(), nullptr, pixel, sizeof(pixel)) == 0);
        return texture;
    }
};

ecs::Entity addObject(ecs::Registry& world, const TexturePtr& texture,
                      math::Vector2 position, ColliderComponent collider) {
    const auto entity = world.createEntity();
    world.addComponent<TransformComponent>(entity, position);
    world.addComponent<ColliderComponent>(entity, collider);
    auto& sprite = world.addComponent<SpriteComponent>(entity);
    sprite.texture = texture;
    sprite.source = {0, 0, 1, 1};
    sprite.size = {160.0f, 160.0f};
    return entity;
}

TEST_CASE_METHOD(DisplaySDLFixture, "Sprites and colliders scale together across resolutions",
                 "[integration][resolution]") {
    struct Example {
        int width, height;
        SDL_Point center, corner;
    };
    const Example examples[]{
        {1280, 720, {640, 360}, {560, 280}},
        {1920, 1080, {960, 540}, {840, 420}},
        {2560, 1440, {1280, 720}, {1120, 560}},
        {2560, 1080, {1280, 540}, {1160, 420}},
        {800, 600, {400, 300}, {350, 250}},
        {1024, 768, {512, 384}, {448, 320}}
    };

    for (const auto& example : examples) {
        CAPTURE(example.width, example.height);
        DisplayCanvas canvas(example.width, example.height);
        ecs::Registry world;
        const auto entity = addObject(world, canvas.whiteTexture(), {640.0f, 360.0f},
                                      ColliderComponent::square(160.0f));
        REQUIRE(canvas.app.settings.width == example.width);
        REQUIRE(canvas.app.settings.height == example.height);
        REQUIRE(canvas.app.getUISize().wight == 1280);
        REQUIRE(canvas.app.getUISize().height == 720);

        renderSprites(world, App::renderer);
        REQUIRE(canvas.isColorAt(example.center.x, example.center.y, white));
        REQUIRE(canvas.isColorAt(example.corner.x, example.center.y, white));
        REQUIRE(canvas.isColorAt(example.corner.x - 3, example.center.y, black));
        renderColliderDebug(world, App::renderer, true);
        REQUIRE(canvas.isColorAt(example.corner.x, example.center.y, outline));
        REQUIRE(canvas.isColorAt(example.center.x, example.corner.y, outline));
        REQUIRE(canvas.isColorAt(example.center.x, example.center.y, white));

        // Subsequent UI/world drawing must still use the original logical projection.
        SDL_SetRenderDrawColor(App::renderer, 200, 100, 50, 255);
        const SDL_Rect marker{632, 352, 16, 16};
        REQUIRE(SDL_RenderFillRect(App::renderer, &marker) == 0);
        REQUIRE(canvas.isColorAt(example.center.x, example.center.y, SDL_Color{200, 100, 50, 255}));

        // Scaling changes the projection, preserving gameplay coordinates and sizes.
        REQUIRE(world.getComponent<TransformComponent>(entity).position.x == 640.0f);
        REQUIRE(world.getComponent<SpriteComponent>(entity).size.x == 160.0f);
        REQUIRE(world.getComponent<ColliderComponent>(entity).size.x == 160.0f);
    }
}

TEST_CASE_METHOD(DisplaySDLFixture, "Circles and polygons preserve proportions with letterboxing",
                 "[integration][resolution]") {
    DisplayCanvas canvas(800, 600); // A 16:9 game occupies 800 x 450, with 75px bars.
    ecs::Registry world;
    const auto texture = canvas.whiteTexture();
    addObject(world, texture, {320.0f, 360.0f}, ColliderComponent::circle(80.0f));
    addObject(world, texture, {960.0f, 360.0f}, ColliderComponent::polygon({
        {-80.0f, 80.0f}, {0.0f, -80.0f}, {80.0f, 80.0f}
    }));

    // Fill the logical viewport to distinguish its edges from the black bars.
    SDL_SetRenderDrawColor(App::renderer, 20, 30, 40, 255);
    const SDL_Rect background{0, 0, 1280, 720};
    REQUIRE(SDL_RenderFillRect(App::renderer, &background) == 0);
    renderSprites(world, App::renderer);
    renderColliderDebug(world, App::renderer, true);
    REQUIRE(canvas.isColorAt(200, 300, white));
    REQUIRE(canvas.isColorAt(150, 300, outline));
    REQUIRE(canvas.isColorAt(200, 250, outline));
    REQUIRE(canvas.isColorAt(600, 300, white));
    REQUIRE(canvas.isColorAt(600, 250, outline));
    REQUIRE(canvas.isColorAt(550, 350, outline));
    REQUIRE(canvas.isColorAt(400, 74, black));
    REQUIRE(canvas.isColorAt(400, 75, SDL_Color{20, 30, 40, 255}));
    REQUIRE(canvas.isColorAt(400, 524, SDL_Color{20, 30, 40, 255}));
    REQUIRE(canvas.isColorAt(400, 525, black));
}

TEST_CASE_METHOD(DisplaySDLFixture, "Resolution changes preserve logical coordinates and mouse input",
                 "[integration][resolution]") {
    App app;
    REQUIRE(App::renderer == nullptr);
    app.window = SDL_CreateWindow("Resize test", 0, 0, 1280, 720, SDL_WINDOW_HIDDEN);
    REQUIRE(app.window != nullptr);
    App::renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_SOFTWARE);
    REQUIRE(App::renderer != nullptr);
    app.syncDisplaySettings();

    for (const auto size : {WindowResolution{1920, 1080}, {800, 600}, {1280, 720}}) {
        CAPTURE(size.width, size.height);
        REQUIRE(app.setResolution(size.width, size.height));
        app.handleEvents();
        REQUIRE(app.getUISize().wight == 1280);
        REQUIRE(app.getUISize().height == 720);
        float logicalX = 0.0f, logicalY = 0.0f;
        SDL_RenderWindowToLogical(App::renderer, size.width / 2, size.height / 2, &logicalX, &logicalY);
        REQUIRE(std::abs(logicalX - 640.0f) < 0.01f);
        REQUIRE(std::abs(logicalY - 360.0f) < 0.01f);

        SDL_Event click{};
        click.type = SDL_MOUSEBUTTONDOWN;
        click.button.windowID = SDL_GetWindowID(app.window);
        click.button.button = SDL_BUTTON_LEFT;
        click.button.x = size.width / 2;
        click.button.y = size.height / 2;
        REQUIRE(SDL_PushEvent(&click) == 1);
        bool received = false;
        while (SDL_PollEvent(&click)) {
            if (click.type != SDL_MOUSEBUTTONDOWN) continue;
            received = true;
            REQUIRE(click.button.x == 640);
            REQUIRE(click.button.y == 360);
        }
        REQUIRE(received);
    }
}

} // namespace
