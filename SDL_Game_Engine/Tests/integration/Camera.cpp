#include <catch_amalgamated.hpp>
#define SDL_MAIN_HANDLED
#include "../../Engine/ECS/CameraSystem.h"
#include "../../Engine/ECS/Components/CameraComponent.h"
#include "../../Engine/ECS/Components/ColliderComponent.h"
#include "../../Engine/ECS/Components/SpriteComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/Renderer/SpriteRenderer.h"
#include "../../Engine/Renderer/ColliderDebugRenderer.h"
#include "../../Engine/Scenes/GameScene.h"
#include "../../Engine/World/Map.h"
#include <SDL_image.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>

namespace {
struct DrawCalls {
    int maps = 0, sprites = 0;
    bool hasSource = false;
    SDL_Rect source{};
    SDL_FRect destination{};
};
DrawCalls* recording = nullptr;
struct Capture {
    explicit Capture(DrawCalls& calls) { recording = &calls; }
    ~Capture() { recording = nullptr; }
};
}

extern "C" int SDLCALL __real_SDL_RenderCopyF(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_FRect*);
extern "C" int SDLCALL __wrap_SDL_RenderCopyF(SDL_Renderer* renderer, SDL_Texture* texture,
                                             const SDL_Rect* source, const SDL_FRect* destination) {
    if (recording) {
        ++recording->maps;
        recording->hasSource = source != nullptr;
        if (source) recording->source = *source;
        if (destination) recording->destination = *destination;
    }
    return __real_SDL_RenderCopyF(renderer, texture, source, destination);
}
extern "C" int SDLCALL __real_SDL_RenderCopyExF(SDL_Renderer*, SDL_Texture*, const SDL_Rect*,
    const SDL_FRect*, double, const SDL_FPoint*, SDL_RendererFlip);
extern "C" int SDLCALL __wrap_SDL_RenderCopyExF(SDL_Renderer* renderer, SDL_Texture* texture,
    const SDL_Rect* source, const SDL_FRect* destination, double angle,
    const SDL_FPoint* center, SDL_RendererFlip flip) {
    if (recording) ++recording->sprites;
    return __real_SDL_RenderCopyExF(renderer, texture, source, destination, angle, center, flip);
}

namespace {
struct CameraSDLFixture {
    CameraSDLFixture() {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
        REQUIRE((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != 0);
    }
    ~CameraSDLFixture() { IMG_Quit(); SDL_Quit(); }
};

class CameraCanvas {
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface{nullptr, SDL_FreeSurface};
public:
    App app;
    CameraCanvas() {
        REQUIRE(App::renderer == nullptr);
        surface.reset(SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_RGBA32));
        REQUIRE(surface != nullptr);
        App::renderer = SDL_CreateSoftwareRenderer(surface.get());
        REQUIRE(App::renderer != nullptr);
        SDL_SetRenderDrawColor(App::renderer, 0, 0, 0, 255);
        REQUIRE(SDL_RenderClear(App::renderer) == 0);
    }
    bool isColorAt(int x, int y, SDL_Color expected) const {
        const SDL_Rect area{x, y, 1, 1};
        std::array<Uint8, 4> pixel{};
        REQUIRE(SDL_RenderReadPixels(App::renderer, &area, SDL_PIXELFORMAT_RGBA32, pixel.data(), 4) == 0);
        return pixel[0] == expected.r && pixel[1] == expected.g && pixel[2] == expected.b;
    }
    TexturePtr whiteTexture() const {
        TexturePtr texture{SDL_CreateTexture(App::renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, 1, 1), SDL_DestroyTexture};
        REQUIRE(texture != nullptr);
        const Uint8 pixel[]{255, 255, 255, 255};
        REQUIRE(SDL_UpdateTexture(texture.get(), nullptr, pixel, 4) == 0);
        return texture;
    }
};

TEST_CASE("Camera follows a target and manual movement detaches it", "[integration][camera]") {
    ecs::Registry world;
    const auto player = world.createEntity();
    auto& transform = world.addComponent<TransformComponent>(player, math::Vector2{1000.0f, 800.0f});
    auto& camera = world.addComponent<CameraComponent>(world.createEntity());
    camera.size = {200.0f, 100.0f};
    camera.target = player;
    camera.following = true;
    camera.speed = 100.0f;
    updateCameras(world);
    REQUIRE(camera.position.x == 900.0f);
    REQUIRE(camera.position.y == 750.0f);
    camera.move({1.0f, 1.0f}, 0.5f);
    REQUIRE_FALSE(camera.following);
    REQUIRE(std::abs(camera.position.x - 935.3553f) < 0.001f);
    REQUIRE(std::abs(camera.position.y - 785.3553f) < 0.001f);
    transform.position.x = 1200.0f;
    updateCameras(world);
    REQUIRE(std::abs(camera.position.x - 935.3553f) < 0.001f);
    camera.following = true;
    updateCameras(world);
    REQUIRE(camera.position.x == 1100.0f);
    world.destroyEntity(player);
    updateCameras(world);
    REQUIRE_FALSE(camera.following);
    REQUIRE(camera.position.x == 1100.0f);
}

TEST_CASE("Camera movement respects map bounds and invalid time steps", "[integration][camera]") {
    CameraComponent camera;
    camera.bounded = true;
    camera.bounds = {0, 0, 5120, 5120};
    camera.position = {5000, -50};
    camera.clampToBounds();
    REQUIRE(camera.position.x == 3840.0f);
    REQUIRE(camera.position.y == 0.0f);
    camera.speed = 200;
    camera.move({-1, 0}, 0.5f);
    REQUIRE(camera.position.x == 3740.0f);
    for (float dt : {-1.0f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
        camera.move({1, 0}, dt);
        REQUIRE(camera.position.x == 3740.0f);
    }
    camera.size = {200, 200};
    camera.bounds = {0, 0, 100, 100};
    camera.clampToBounds();
    REQUIRE(camera.position.x == -50.0f);
    REQUIRE(camera.position.y == -50.0f);
    REQUIRE(camera.isVisible({0, 0, 10, 10}));
    REQUIRE_FALSE(camera.isVisible({200, 200, 10, 10}));
}

std::filesystem::path createMap() {
    std::filesystem::create_directories("CameraFixtures");
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> image{
        SDL_CreateRGBSurfaceWithFormat(0, 4, 4, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface};
    REQUIRE(image != nullptr);
    const std::array<SDL_Color, 4> colors{{{255, 0, 0, 255}, {0, 255, 0, 255},
                                         {0, 0, 255, 255}, {255, 255, 0, 255}}};
    nlohmann::json frames = nlohmann::json::array();
    for (int i = 0; i < 4; ++i) {
        const SDL_Rect area{(i % 2) * 2, (i / 2) * 2, 2, 2};
        const auto color = colors[i];
        REQUIRE(SDL_FillRect(image.get(), &area, SDL_MapRGBA(image->format,
            color.r, color.g, color.b, color.a)) == 0);
        frames.push_back({{"frame", {{"x", area.x}, {"y", area.y}, {"w", 2}, {"h", 2}}}});
    }
    REQUIRE(SDL_SaveBMP(image.get(), "CameraFixtures/map.bmp") == 0);
    const nlohmann::json data{{"frames", frames},
        {"meta", {{"image", "map.bmp"}, {"size", {{"w", 4}, {"h", 4}}}}}};
    std::ofstream output("CameraFixtures/map.json");
    output << data.dump();
    REQUIRE(output.good());
    return "CameraFixtures/map.json";
}

TEST_CASE_METHOD(CameraSDLFixture, "Map sends only visible source pixels to SDL", "[integration][camera]") {
    CameraCanvas canvas;
    REQUIRE(SDL_RenderSetLogicalSize(App::renderer, 32, 32) == 0);
    Map map;
    REQUIRE(map.load(createMap()));
    map.scale = 16;
    CameraComponent camera;
    camera.position = {16, 16};
    camera.size = {32, 32};
    DrawCalls calls;
    Capture capture(calls);
    map.draw(App::renderer, &camera);
    REQUIRE(calls.maps == 1);
    REQUIRE(calls.hasSource);
    REQUIRE(calls.source.x == 1);
    REQUIRE(calls.source.y == 1);
    REQUIRE(calls.source.w == 2);
    REQUIRE(calls.source.h == 2);
    REQUIRE(calls.destination.x == 0.0f);
    REQUIRE(calls.destination.w == 32.0f);
    REQUIRE(canvas.isColorAt(2, 2, SDL_Color{255, 0, 0, 255}));
    REQUIRE(canvas.isColorAt(34, 34, SDL_Color{255, 255, 0, 255}));
    calls = {};
    camera.position = {20, 20};
    map.draw(App::renderer, &camera);
    REQUIRE(calls.maps == 1);
    REQUIRE(calls.source.x == 1);
    REQUIRE(calls.source.w == 3);
    REQUIRE(calls.destination.x == -4.0f);
    calls = {};
    camera.position = {100, 100};
    map.draw(App::renderer, &camera);
    REQUIRE(calls.maps == 0);
}

ecs::Entity addSprite(ecs::Registry& world, const TexturePtr& texture, math::Vector2 position) {
    const auto entity = world.createEntity();
    world.addComponent<TransformComponent>(entity, position);
    auto& sprite = world.addComponent<SpriteComponent>(entity);
    sprite.texture = texture;
    sprite.source = {0, 0, 1, 1};
    sprite.size = {16, 16};
    world.addComponent<ColliderComponent>(entity, ColliderComponent::square(16));
    return entity;
}

TEST_CASE_METHOD(CameraSDLFixture, "Camera aligns sprites and colliders and skips hidden objects",
                 "[integration][camera]") {
    CameraCanvas canvas;
    ecs::Registry world;
    const auto texture = canvas.whiteTexture();
    const auto visible = addSprite(world, texture, {132, 232});
    addSprite(world, texture, {500, 500});
    CameraComponent camera;
    camera.position = {100, 200};
    camera.size = {64, 64};
    DrawCalls calls;
    Capture capture(calls);
    renderSprites(world, App::renderer, &camera);
    REQUIRE(calls.sprites == 1);
    REQUIRE(canvas.isColorAt(32, 32, SDL_Color{255, 255, 255, 255}));
    renderColliderDebug(world, App::renderer, true, &camera);
    REQUIRE(canvas.isColorAt(24, 32, SDL_Color{64, 255, 128, 255}));
    REQUIRE(world.getComponent<TransformComponent>(visible).position.x == 132.0f);
    SDL_SetRenderDrawColor(App::renderer, 255, 0, 0, 255);
    const SDL_Rect hud{0, 0, 8, 8};
    REQUIRE(SDL_RenderFillRect(App::renderer, &hud) == 0);
    REQUIRE(canvas.isColorAt(2, 2, SDL_Color{255, 0, 0, 255}));
}

class CameraScene : public GameScene {
    ecs::Entity player;
protected:
    void updateWorld(App&) override { world.getComponent<TransformComponent>(player).position.x += 10.0f; }
public:
    explicit CameraScene(const TexturePtr& texture) {
        player = addSprite(world, texture, {100, 200});
        cameraEntity = world.createEntity();
        auto& view = world.addComponent<CameraComponent>(cameraEntity);
        view.size = {64, 64};
        view.following = true;
        view.target = player;
    }
    math::Vector2 cameraPosition() { return camera()->position; }
};

TEST_CASE_METHOD(CameraSDLFixture, "Scene camera follows after world movement", "[integration][camera]") {
    CameraCanvas canvas;
    CameraScene scene(canvas.whiteTexture());
    scene.update(canvas.app);
    REQUIRE(scene.cameraPosition().x == 78.0f);
    REQUIRE(scene.cameraPosition().y == 168.0f);
    scene.render(canvas.app);
    REQUIRE(canvas.isColorAt(32, 32, SDL_Color{255, 255, 255, 255}));
}
} // namespace
