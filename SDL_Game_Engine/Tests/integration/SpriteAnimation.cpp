#include <catch_amalgamated.hpp>
#define SDL_MAIN_HANDLED
#include "../../Engine/ECS/ECS.h"
#include "../../Engine/ECS/AnimationSystem.h"
#include "../../Engine/ECS/Components/AnimationComponent.h"
#include "../../Engine/ECS/Components/SpriteComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/Renderer/SpriteRenderer.h"
#include "../../Engine/Renderer/TextureManager.h"
#include "../../Engine/Scenes/GameScene.h"

#include <array>
#include <limits>
#include <memory>

namespace {

constexpr SDL_Color black{0, 0, 0, 255};
constexpr SDL_Color red{255, 0, 0, 255};
constexpr SDL_Color green{0, 255, 0, 255};
constexpr SDL_Color blue{0, 0, 255, 255};
constexpr SDL_Color yellow{255, 255, 0, 255};

struct SpriteSDLFixture {
    SpriteSDLFixture() {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
    }
    ~SpriteSDLFixture() { SDL_Quit(); }
};

class SpriteCanvas {
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface{nullptr, SDL_FreeSurface};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer{nullptr, SDL_DestroyRenderer};

public:
    SpriteCanvas() {
        surface.reset(SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_RGBA32));
        REQUIRE(surface != nullptr);
        renderer.reset(SDL_CreateSoftwareRenderer(surface.get()));
        REQUIRE(renderer != nullptr);
        clear();
    }

    SDL_Renderer* get() const { return renderer.get(); }

    void clear() const {
        SDL_SetRenderDrawColor(get(), 0, 0, 0, 255);
        REQUIRE(SDL_RenderClear(get()) == 0);
    }

    bool isColorAt(int x, int y, SDL_Color expected) const {
        const SDL_Rect area{x, y, 1, 1};
        std::array<Uint8, 4> pixel{};
        REQUIRE(SDL_RenderReadPixels(get(), &area, SDL_PIXELFORMAT_RGBA32, pixel.data(), 4) == 0);
        return pixel[0] == expected.r && pixel[1] == expected.g &&
               pixel[2] == expected.b && pixel[3] == expected.a;
    }

    TexturePtr atlas(int* destructions = nullptr) const {
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> image{
            SDL_CreateRGBSurfaceWithFormat(0, 4, 4, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface};
        REQUIRE(image != nullptr);
        const std::array<SDL_Color, 4> colors{red, green, blue, yellow};
        for (int i = 0; i < 4; ++i) {
            const SDL_Rect area{(i % 2) * 2, (i / 2) * 2, 2, 2};
            const auto color = colors[i];
            REQUIRE(SDL_FillRect(image.get(), &area,
                SDL_MapRGBA(image->format, color.r, color.g, color.b, color.a)) == 0);
        }
        TexturePtr texture{SDL_CreateTextureFromSurface(get(), image.get()),
            [destructions](SDL_Texture* value) {
                if (destructions) ++*destructions;
                SDL_DestroyTexture(value);
            }};
        REQUIRE(texture != nullptr);
        REQUIRE(SDL_SetTextureScaleMode(texture.get(), SDL_ScaleModeNearest) == 0);
        return texture;
    }
};

ecs::Entity addSprite(ecs::Registry& world, const TexturePtr& texture) {
    const auto entity = world.createEntity();
    world.addComponent<TransformComponent>(entity, math::Vector2{32.0f, 24.0f});
    auto& sprite = world.addComponent<SpriteComponent>(entity);
    sprite.texture = texture;
    sprite.source = {0, 0, 2, 2};
    sprite.size = {16.0f, 8.0f};
    return entity;
}

TEST_CASE_METHOD(SpriteSDLFixture, "Sprite crop, size, center and offset", "[integration][sprite]") {
    SpriteCanvas canvas;
    ecs::Registry world;
    const auto entity = addSprite(world, canvas.atlas());
    auto& sprite = world.getComponent<SpriteComponent>(entity);
    sprite.source = {2, 2, 2, 2};
    sprite.offset = {4.0f, 6.0f};

    renderSprites(world, canvas.get());
    REQUIRE(canvas.isColorAt(28, 26, yellow));
    REQUIRE(canvas.isColorAt(43, 33, yellow));
    REQUIRE(canvas.isColorAt(27, 26, black));
    REQUIRE(canvas.isColorAt(44, 33, black));
    REQUIRE(canvas.isColorAt(28, 25, black));
    REQUIRE(canvas.isColorAt(43, 34, black));

    world.getComponent<TransformComponent>(entity).position.x += 10.0f;
    canvas.clear();
    renderSprites(world, canvas.get());
    REQUIRE(canvas.isColorAt(38, 26, yellow));
    REQUIRE(canvas.isColorAt(28, 26, black));
}

TEST_CASE_METHOD(SpriteSDLFixture, "Sprite flip and visibility", "[integration][sprite]") {
    SpriteCanvas canvas;
    ecs::Registry world;
    const auto entity = addSprite(world, canvas.atlas());
    auto& sprite = world.getComponent<SpriteComponent>(entity);
    sprite.source = {0, 0, 4, 4};
    sprite.size = {16.0f, 16.0f};
    renderSprites(world, canvas.get());
    REQUIRE(canvas.isColorAt(26, 18, red));
    REQUIRE(canvas.isColorAt(38, 18, green));

    sprite.flip = SDL_FLIP_HORIZONTAL;
    canvas.clear();
    renderSprites(world, canvas.get());
    REQUIRE(canvas.isColorAt(26, 18, green));
    REQUIRE(canvas.isColorAt(38, 18, red));

    sprite.flip = SDL_FLIP_VERTICAL;
    canvas.clear();
    renderSprites(world, canvas.get());
    REQUIRE(canvas.isColorAt(26, 18, blue));
    REQUIRE(canvas.isColorAt(38, 18, yellow));

    sprite.visible = false;
    canvas.clear();
    renderSprites(world, canvas.get());
    REQUIRE(canvas.isColorAt(26, 18, black));
}

TEST_CASE_METHOD(SpriteSDLFixture, "Animated sprite timing and pause", "[integration][sprite][animation]") {
    SpriteCanvas canvas;
    ecs::Registry world;
    const auto entity = addSprite(world, canvas.atlas());
    auto& animation = world.addComponent<AnimationComponent>(entity);
    animation.row = 1;
    animation.frameCount = 2;
    animation.frameDuration = 0.1;

    updateAnimations(world, 0.5); // Five frames, with no remaining partial frame.
    updateAnimations(world, 0.05);
    REQUIRE(animation.currentFrame == 1);
    REQUIRE(std::abs(animation.elapsed - 0.05) < 1e-12);
    renderSprites(world, canvas.get());
    REQUIRE(canvas.isColorAt(32, 24, yellow));

    animation.playing = false;
    updateAnimations(world, 50.0);
    REQUIRE(animation.currentFrame == 1);
    REQUIRE(std::abs(animation.elapsed - 0.05) < 1e-12);

    animation.playing = true;
    updateAnimations(world, 0.05);
    REQUIRE(animation.currentFrame == 0);
    canvas.clear();
    renderSprites(world, canvas.get());
    REQUIRE(canvas.isColorAt(32, 24, blue));
}

TEST_CASE("Animation completion, restart and long updates", "[integration][sprite][animation]") {
    ecs::Registry world;
    const auto entity = world.createEntity();
    auto& sprite = world.addComponent<SpriteComponent>(entity);
    auto& animation = world.addComponent<AnimationComponent>(entity);
    animation.row = 2;
    animation.frameCount = 4;
    animation.frameDuration = 0.125;
    animation.loop = false;

    updateAnimations(world, 0.5);
    REQUIRE(animation.currentFrame == 3);
    REQUIRE_FALSE(animation.playing);
    REQUIRE(sprite.source.x == 192);
    REQUIRE(sprite.source.y == 128);
    updateAnimations(world, 100.0);
    REQUIRE(animation.currentFrame == 3);

    animation.restart();
    REQUIRE(animation.currentFrame == 0);
    REQUIRE(animation.elapsed == 0.0);
    REQUIRE(animation.playing);
    animation.loop = true;
    updateAnimations(world, 1000000.1875); // 8,000,001 frames and half a frame.
    REQUIRE(animation.currentFrame == 1);
    REQUIRE(animation.elapsed == 0.0625);
    REQUIRE(sprite.source.x == 64);
}

TEST_CASE_METHOD(SpriteSDLFixture, "Invalid or incomplete sprites and animations", "[integration][sprite][animation]") {
    SpriteCanvas canvas;
    ecs::Registry world;
    const auto entity = addSprite(world, canvas.atlas());
    auto& sprite = world.getComponent<SpriteComponent>(entity);
    auto& animation = world.addComponent<AnimationComponent>(entity);
    animation.frameCount = 2;
    animation.frameDuration = 0.125;

    for (double dt : {-1.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        updateAnimations(world, dt);
        REQUIRE(animation.currentFrame == 0);
        REQUIRE(animation.elapsed == 0.0);
    }
    for (double duration : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        animation.frameDuration = duration;
        updateAnimations(world, 1.0);
        REQUIRE(animation.currentFrame == 0);
    }
    animation.frameDuration = 0.125;
    animation.row = std::numeric_limits<int>::max();
    updateAnimations(world, 0.125); // The source y coordinate would overflow.
    REQUIRE(animation.currentFrame == 0);
    REQUIRE(sprite.source.y == 0);

    world.addComponent<SpriteComponent>(world.createEntity());
    world.addComponent<TransformComponent>(world.createEntity());
    auto& orphan = world.addComponent<AnimationComponent>(world.createEntity());
    orphan.frameCount = 2;
    updateAnimations(world, 1.0);
    REQUIRE(orphan.currentFrame == 0);

    for (SDL_Rect source : {SDL_Rect{-1, 0, 2, 2}, SDL_Rect{0, 0, 0, 2},
                            SDL_Rect{3, 0, 2, 2}, SDL_Rect{0, 0, 5, 2}}) {
        sprite.source = source;
        renderSprites(world, canvas.get());
        REQUIRE(canvas.isColorAt(32, 24, black));
    }
    sprite.source = {0, 0, 2, 2};
    for (float size : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
        sprite.size.x = size;
        renderSprites(world, canvas.get());
        REQUIRE(canvas.isColorAt(32, 24, black));
    }
    renderSprites(world, nullptr);
}

TEST_CASE_METHOD(SpriteSDLFixture, "Sprites release their shared texture once", "[integration][sprite]") {
    SpriteCanvas canvas;
    int destructions = 0;
    ecs::Registry world;
    auto texture = canvas.atlas(&destructions);
    const auto first = addSprite(world, texture);
    const auto second = addSprite(world, texture);
    texture.reset();
    world.destroyEntity(first);
    REQUIRE(destructions == 0);
    renderSprites(world, canvas.get());
    REQUIRE(canvas.isColorAt(32, 24, red));
    world.removeComponent<SpriteComponent>(second);
    REQUIRE(destructions == 1);
    world.clear();
    REQUIRE(destructions == 1);

    REQUIRE_FALSE(TextureManager::LoadSharedTexture(""));
}

class AnimatedTestScene : public GameScene {
    ecs::Entity entity;

protected:
    void updateWorld(App&) override { animation().row = 1; }

public:
    explicit AnimatedTestScene(const TexturePtr& texture) {
        entity = addSprite(world, texture);
        auto& component = world.addComponent<AnimationComponent>(entity);
        component.frameCount = 2;
        component.frameDuration = 0.125;
    }

    AnimationComponent& animation() { return world.getComponent<AnimationComponent>(entity); }
};

TEST_CASE_METHOD(SpriteSDLFixture, "Game scenes advance and render sprites", "[integration][sprite][animation]") {
    App app;
    SpriteCanvas canvas;
    struct AppContext {
        SDL_Renderer* previousRenderer = App::renderer;
        float previousDeltaTime = App::deltaTime;
        explicit AppContext(SDL_Renderer* renderer) {
            App::renderer = renderer;
            App::deltaTime = 0.125f;
        }
        ~AppContext() {
            App::renderer = previousRenderer;
            App::deltaTime = previousDeltaTime;
        }
    } context(canvas.get());
    AnimatedTestScene scene(canvas.atlas());

    scene.update(app);
    REQUIRE(scene.animation().currentFrame == 1);
    scene.render(app);
    REQUIRE(canvas.isColorAt(32, 24, yellow));
    REQUIRE(canvas.isColorAt(0, 0, SDL_Color{24, 28, 36, 255}));
}

} // namespace
