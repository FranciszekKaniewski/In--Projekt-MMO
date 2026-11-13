#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct ServerConfig {
    std::string host = "localhost";
    std::uint16_t port = 4000;
    std::string caFile = "server-ca.crt"; // Relative to the settings file.
    int timeoutMs = 5000;
};

struct CharacterSummary {
    std::int64_t id = 0;
    std::string name;
};

struct CharacterState : CharacterSummary {
    std::string mapKey;
    float x = 0, y = 0;
    int facing = 4;
    std::int64_t level = 1, experience = 0;
};

struct WorldPlayer : CharacterSummary {
    float x = 0, y = 0;
    int facing = 4;
    bool moving = false;
};

using RequestId = std::uint64_t;

struct ServerReply {
    enum class Type { Login, Characters, CharacterSelected, MapData, WorldJoined, Move, WorldLeft, Error };
    RequestId request = 0; // Zero denotes an unsolicited connection/session event.
    Type type = Type::Error;
    std::string error;
    std::vector<CharacterSummary> characters;
    bool hasMore = false;
    std::optional<CharacterState> character;
    std::string mapData;
};

// The application owns the client, so changing scenes preserves the TLS session.
// Requests return immediately; scenes consume their own replies in update().
class GameClient {
public:
    virtual ~GameClient() = default;
    virtual RequestId login(std::string username, std::string password) = 0;
    virtual RequestId listCharacters(std::int64_t afterId = 0) = 0;
    virtual RequestId selectCharacter(std::int64_t id) = 0;
    virtual RequestId downloadMap() = 0;
    virtual RequestId enterWorld() = 0;
    virtual RequestId moveInput(int dx, int dy) = 0;
    virtual RequestId leaveWorld() = 0;
    virtual std::optional<std::vector<WorldPlayer>> pollWorld() = 0;
    virtual std::optional<ServerReply> poll() = 0;
    virtual bool isAuthenticated() const = 0;
    virtual std::optional<CharacterState> selectedCharacter() const = 0;
    virtual void disconnect() = 0;
};

std::unique_ptr<GameClient> makeGameClient(ServerConfig config);
std::string serverErrorText(const std::string& code);
