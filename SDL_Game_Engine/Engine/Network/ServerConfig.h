#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct ServerConfig {
    std::string host;
    std::uint16_t port = 0;
    std::string caFile;
    int timeoutMs = 0;
};

ServerConfig loadServerConfig(const std::filesystem::path& envPath);
