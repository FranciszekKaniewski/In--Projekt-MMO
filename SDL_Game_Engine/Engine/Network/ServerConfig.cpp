#include "ServerConfig.h"

#include <charconv>
#include <fstream>
#include <map>
#include <stdexcept>
#include <utility>

namespace {
std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

int number(const std::string& text, const char* key, int maximum) {
    const auto end = text.data() + text.size();
    int value = 0;
    const auto result = std::from_chars(text.data(), end, value);
    if(result.ec != std::errc{} || result.ptr != end || value < 1 || value > maximum)
        throw std::runtime_error(std::string("Invalid .env setting ") + key);
    return value;
}
}

ServerConfig loadServerConfig(const std::filesystem::path& envPath) {
    std::ifstream file(envPath, std::ios::binary);
    if(!file) throw std::runtime_error("Cannot open " + envPath.u8string());

    std::map<std::string, std::string> values;
    for(std::string line; std::getline(file, line);) {
        if(line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
        const auto separator = line.find('=');
        if(separator == std::string::npos) continue;
        const auto key = trim(line.substr(0, separator));
        if(key != "GAME_HOST" && key != "GAME_PORT" && key != "GAME_CAFILE" && key != "GAME_TIMEOUT_MS") continue;
        auto value = trim(line.substr(separator + 1));
        if(value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.front() == value.back())
            value = value.substr(1, value.size() - 2);
        values[key] = std::move(value);
    }
    if(!file.eof()) throw std::runtime_error("Cannot read " + envPath.u8string());

    const auto required = [&](const char* key) -> const std::string& {
        const auto found = values.find(key);
        if(found == values.end() || found->second.empty())
            throw std::runtime_error(std::string("Missing ") + key + " in " + envPath.u8string());
        return found->second;
    };
    const auto& host = required("GAME_HOST");
    if(host.size() > 253) throw std::runtime_error("Invalid .env setting GAME_HOST");
    const auto port = number(required("GAME_PORT"), "GAME_PORT", 65535);
    const auto ca = (envPath.parent_path() / std::filesystem::u8path(required("GAME_CAFILE"))).lexically_normal();
    const auto timeout = number(required("GAME_TIMEOUT_MS"), "GAME_TIMEOUT_MS", 30000);
    return {host, static_cast<std::uint16_t>(port), ca.u8string(), timeout};
}
