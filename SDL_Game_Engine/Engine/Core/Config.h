#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "../Network/GameClient.h"

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
    ServerConfig server;
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
    if (j.contains("server")) {
        const auto& server = j.at("server");
        cfg.server.host = server.value("host", "localhost");
        const int port = server.value("port", 4000);
        cfg.server.caFile = server.value("caFile", "server-ca.crt");
        cfg.server.timeoutMs = server.value("timeoutMs", 5000);
        if(cfg.server.host.empty() || cfg.server.host.size() > 253 || port < 1 || port > 65535 ||
           cfg.server.caFile.empty() || cfg.server.timeoutMs < 1 || cfg.server.timeoutMs > 30000)
            throw std::runtime_error("Niepoprawna konfiguracja serwera");
        cfg.server.port = static_cast<std::uint16_t>(port);
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
        {"audio", {{"volume", cfg.audio.volume}, {"muted", cfg.audio.muted}}},
        {"server", {{"host", cfg.server.host}, {"port", cfg.server.port},
                    {"caFile", cfg.server.caFile}, {"timeoutMs", cfg.server.timeoutMs}}}
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
