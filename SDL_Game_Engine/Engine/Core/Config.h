#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

struct WindowConfig {
    std::string title;
    int width, height;
    bool fullscreen;
};

struct GameConfig {
    int fps;
    bool devMode;
};

struct AudioConfig {
    int volume = 100;
    bool muted = false;
};

struct AppConfig {
    WindowConfig window;
    GameConfig game;
    AudioConfig audio;
};

inline AppConfig loadConfig(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Nie mozna otworzyc: " + path.string());

    nlohmann::json j = nlohmann::json::parse(file);

    AppConfig cfg;
    cfg.window.title = j["window"]["title"];
    cfg.window.width = j["window"]["width"];
    cfg.window.height = j["window"]["height"];
    cfg.window.fullscreen = j["window"]["fullscreen"];
    cfg.game.fps = j["game"]["fps"];
    cfg.game.devMode = j["game"]["devMode"];
    if (j.contains("audio")) {
        cfg.audio.volume = j["audio"].value("volume", 100);
        cfg.audio.muted = j["audio"].value("muted", false);
    }
    return cfg;
}

inline void saveConfig(const std::filesystem::path& path, const AppConfig& cfg) {
    const nlohmann::json j = {
        {"window", {
            {"title", cfg.window.title},
            {"width", cfg.window.width},
            {"height", cfg.window.height},
            {"fullscreen", cfg.window.fullscreen}
        }},
        {"game", {{"fps", cfg.game.fps}, {"devMode", cfg.game.devMode}}},
        {"audio", {{"volume", cfg.audio.volume}, {"muted", cfg.audio.muted}}}
    };

    std::filesystem::path temporaryPath = path;
    temporaryPath += ".tmp";
    try {
        std::ofstream file(temporaryPath);
        if (!file.is_open())
            throw std::runtime_error("Nie mozna zapisac: " + temporaryPath.string());

        file << j.dump(2) << '\n';
        file.close();
        if (!file)
            throw std::runtime_error("Blad zapisu: " + temporaryPath.string());

        // Replace the old configuration only after the entire write succeeds.
        std::filesystem::rename(temporaryPath, path);
    } catch (...) {
        std::error_code error;
        std::filesystem::remove(temporaryPath, error);
        throw;
    }
}
