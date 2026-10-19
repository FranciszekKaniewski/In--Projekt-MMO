#include <catch_amalgamated.hpp>
#define SDL_MAIN_HANDLED
#include "../../Engine/Core/App.h"
#include "../../Engine/World/Map.h"
#include <SDL_image.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>

namespace {
using Json = nlohmann::json;

struct MapSDLFixture {
    MapSDLFixture() {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
        REQUIRE((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != 0);
    }
    ~MapSDLFixture() {
        IMG_Quit();
        SDL_Quit();
    }
};

class MapCanvas {
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface{nullptr, SDL_FreeSurface};

public:
    App app;

    MapCanvas(int width = 64, int height = 64) {
        REQUIRE(App::renderer == nullptr);
        surface.reset(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32));
        REQUIRE(surface != nullptr);
        App::renderer = SDL_CreateSoftwareRenderer(surface.get());
        REQUIRE(App::renderer != nullptr);
        clear();
    }

    void clear() const {
        SDL_SetRenderDrawColor(App::renderer, 0, 0, 0, 255);
        REQUIRE(SDL_RenderClear(App::renderer) == 0);
    }

    bool isColorAt(int x, int y, SDL_Color expected) const {
        const SDL_Rect area{x, y, 1, 1};
        std::array<Uint8, 4> pixel{};
        REQUIRE(SDL_RenderReadPixels(App::renderer, &area, SDL_PIXELFORMAT_RGBA32, pixel.data(), 4) == 0);
        return pixel[0] == expected.r && pixel[1] == expected.g &&
               pixel[2] == expected.b && pixel[3] == expected.a;
    }

    void savePreview(const char* path) const {
        REQUIRE(SDL_RenderFlush(App::renderer) == 0);
        REQUIRE(IMG_SavePNG(surface.get(), path) == 0);
    }
};

const std::filesystem::path fixturePath = "MapFixtures";

Json fixture() {
    std::filesystem::create_directories(fixturePath);
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> image{
        SDL_CreateRGBSurfaceWithFormat(0, 4, 4, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface};
    REQUIRE(image != nullptr);
    const std::array<SDL_Color, 4> colors{{{255, 0, 0, 255}, {0, 255, 0, 255},
                                         {0, 0, 255, 255}, {255, 255, 0, 255}}};
    Json frames = Json::array();
    for (int i = 0; i < 4; ++i) {
        const SDL_Rect area{(i % 2) * 2, (i / 2) * 2, 2, 2};
        const auto color = colors[i];
        REQUIRE(SDL_FillRect(image.get(), &area,
            SDL_MapRGBA(image->format, color.r, color.g, color.b, color.a)) == 0);
        frames.push_back({{"frame", {{"x", area.x}, {"y", area.y}, {"w", area.w}, {"h", area.h}}}});
    }
    REQUIRE(SDL_SaveBMP(image.get(), (fixturePath / "colors.bmp").string().c_str()) == 0);
    return {{"frames", frames}, {"meta", {{"image", "colors.bmp"}, {"size", {{"w", 4}, {"h", 4}}}}}};
}

std::filesystem::path save(const Json& data, const char* name = "map.json") {
    const auto path = fixturePath / name;
    std::ofstream output(path);
    output << data.dump();
    REQUIRE(output.good());
    return path;
}

TEST_CASE_METHOD(MapSDLFixture, "Map rendering, placement and logical scaling", "[integration][map]") {
    MapCanvas canvas;
    Map map;
    REQUIRE(map.load(save(fixture())));
    REQUIRE(map.columns() == 2);
    REQUIRE(map.rows() == 2);
    REQUIRE(map.tileCount() == 4);
    REQUIRE(map.tileWidth() == 2);
    map.position = {8.0f, 12.0f};
    map.scale = 8.0f;
    REQUIRE(map.worldSize().x == 32.0f);
    map.draw(App::renderer);
    REQUIRE(canvas.isColorAt(10, 14, SDL_Color{255, 0, 0, 255}));
    REQUIRE(canvas.isColorAt(26, 14, SDL_Color{0, 255, 0, 255}));
    REQUIRE(canvas.isColorAt(10, 30, SDL_Color{0, 0, 255, 255}));
    REQUIRE(canvas.isColorAt(26, 30, SDL_Color{255, 255, 0, 255}));
    REQUIRE(canvas.isColorAt(7, 12, SDL_Color{0, 0, 0, 255}));
    REQUIRE(canvas.isColorAt(40, 12, SDL_Color{0, 0, 0, 255}));

    REQUIRE(SDL_RenderSetLogicalSize(App::renderer, 32, 32) == 0);
    canvas.clear();
    map.position = {4.0f, 4.0f};
    map.scale = 4.0f;
    map.draw(App::renderer);
    REQUIRE(canvas.isColorAt(10, 10, SDL_Color{255, 0, 0, 255}));
    REQUIRE(canvas.isColorAt(26, 26, SDL_Color{255, 255, 0, 255}));

    map.clear();
    canvas.clear();
    map.draw(App::renderer);
    REQUIRE_FALSE(map.isLoaded());
    REQUIRE(map.tileCount() == 0);
    REQUIRE(canvas.isColorAt(10, 10, SDL_Color{0, 0, 0, 255}));
}

TEST_CASE_METHOD(MapSDLFixture, "Map rejects invalid exports without losing the loaded map", "[integration][map]") {
    MapCanvas canvas;
    const auto valid = fixture();
    Map map;
    REQUIRE(map.load(save(valid)));
    const auto rejected = [&](Json changed) {
        REQUIRE_FALSE(map.load(save(changed, "invalid.json")));
        REQUIRE_FALSE(map.error().empty());
        REQUIRE(map.isLoaded());
        REQUIRE(map.tileCount() == 4);
    };
    auto changed = valid;
    changed["frames"][1]["frame"] = changed["frames"][0]["frame"];
    rejected(changed);
    changed = valid;
    changed["frames"][0]["frame"]["x"] = -1;
    rejected(changed);
    changed = valid;
    changed["frames"][0]["rotated"] = true;
    rejected(changed);
    changed = valid;
    changed["frames"].erase(0);
    rejected(changed);
    changed = valid;
    changed["meta"]["size"]["w"] = 4.5;
    rejected(changed);
    changed = valid;
    changed["meta"]["image"] = "missing.png";
    rejected(changed);
    changed = valid;
    changed["meta"]["size"]["w"] = 8;
    for (int y = 0; y < 4; y += 2) for (int x = 4; x < 8; x += 2) {
        changed["frames"].push_back({{"frame", {{"x", x}, {"y", y}, {"w", 2}, {"h", 2}}}});
    }
    rejected(changed); // Grid is valid, but the image is still only 4 x 4.

    REQUIRE_FALSE(map.load(fixturePath / "missing.json"));
    map.scale = 4.0f;
    map.draw(App::renderer);
    REQUIRE(canvas.isColorAt(1, 1, SDL_Color{255, 0, 0, 255}));
    for (float scale : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN()}) {
        canvas.clear();
        map.scale = scale;
        map.draw(App::renderer);
        REQUIRE(canvas.isColorAt(1, 1, SDL_Color{0, 0, 0, 255}));
    }
    map.draw(nullptr);
}

TEST_CASE_METHOD(MapSDLFixture, "Example island map loads its matching PNG", "[integration][map]") {
    MapCanvas canvas(1280, 720);
    Map map;
    REQUIRE(map.load("Assets/Maps/Map_example.json"));
    REQUIRE(map.width() == 1280);
    REQUIRE(map.height() == 1280);
    REQUIRE(map.tileWidth() == 16);
    REQUIRE(map.tileHeight() == 16);
    REQUIRE(map.columns() == 80);
    REQUIRE(map.rows() == 80);
    REQUIRE(map.tileCount() == 6400);
    map.scale = 0.5625f;
    map.position = {280.0f, 0.0f};
    map.draw(App::renderer);

    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> reference{
        IMG_Load("Assets/Maps/Map_example.png"), SDL_FreeSurface};
    REQUIRE(reference != nullptr);
    for (const auto point : {SDL_Point{0, 0}, {640, 640}}) {
        Uint32 pixel = 0;
        const auto* address = static_cast<const Uint8*>(reference->pixels) +
            point.y * reference->pitch + point.x * reference->format->BytesPerPixel;
        std::memcpy(&pixel, address, reference->format->BytesPerPixel);
        SDL_Color expected{};
        SDL_GetRGBA(pixel, reference->format, &expected.r, &expected.g, &expected.b, &expected.a);
        REQUIRE(canvas.isColorAt(280 + static_cast<int>(point.x * map.scale),
                                 static_cast<int>(point.y * map.scale), expected));
    }
    REQUIRE(canvas.isColorAt(279, 0, SDL_Color{0, 0, 0, 255}));
    canvas.savePreview("Map-preview.png");
}

} // namespace
