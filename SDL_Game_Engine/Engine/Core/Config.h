#pragma once
#include <fstream>
#include <string>
#include "../Vendor/nlohmann/json.hpp"

struct WindowConfig {
    std::string title;
    int width, height;
    bool fullscreen;
};

struct GameConfig {
    int fps;
    bool devMode;
};

struct AppConfig {
    WindowConfig window;
    GameConfig game;
};

inline AppConfig loadConfig(const char* path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error(std::string("Nie mozna otworzyc: ") + path);

    nlohmann::json j = nlohmann::json::parse(file);

    AppConfig cfg;
    cfg.window.title = j["window"]["title"];
    cfg.window.width = j["window"]["width"];
    cfg.window.height = j["window"]["height"];
    cfg.window.fullscreen = j["window"]["fullscreen"];
    cfg.game.fps = j["game"]["fps"];
    cfg.game.devMode = j["game"]["devMode"];
    return cfg;
}