#include "GameClient.h"
#include "TlsConnection.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>

namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr std::size_t maxMapBytes = 1024 * 1024;
constexpr std::size_t mapFrameBytes = 4096;
constexpr auto pingInterval = std::chrono::seconds(1);
constexpr auto pingFreshness = std::chrono::seconds(5);

std::int64_t integer(const Json& value, std::int64_t minimum = 1) {
    if(!value.is_number_integer() ||
       (value.is_number_unsigned() && value.get<std::uint64_t>() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())))
        throw NetworkError("invalid_response");
    const auto result = value.get<std::int64_t>();
    if(result < minimum) throw NetworkError("invalid_response");
    return result;
}

std::string text(const Json& value, std::size_t maximum) {
    const auto result = value.get<std::string>();
    if(result.empty() || result.size() > maximum) throw NetworkError("invalid_response");
    return result;
}

bool sessionLost(const std::string& code) {
    return code == "session_expired" || code == "authentication_required";
}

struct Command {
    RequestId id;
    std::uint64_t generation;
    Json body;
};

WorldPlayer decodePlayer(const Json& row) {
    WorldPlayer player;
    player.id = integer(row.at("id"));
    player.name = text(row.at("name"), 128);
    player.x = row.at("position_x").get<float>();
    player.y = row.at("position_y").get<float>();
    const auto facing = integer(row.at("facing"), 0);
    if(!std::isfinite(player.x) || !std::isfinite(player.y) || facing > 7)
        throw NetworkError("invalid_response");
    player.facing = static_cast<int>(facing);
    return player;
}

ServerReply decode(const Json& message, const Command& command) {
    ServerReply reply;
    reply.request = command.id;
    const auto type = text(message.at("type"), 64);
    if(type == "error") {
        reply.error = text(message.at("code"), 64);
        return reply;
    }
    const auto request = command.body.at("type").get<std::string>();
    if(request == "login" && type == "login_ok") {
        integer(message.at("user_id"));
        integer(message.at("expires_in_ms"));
        text(message.at("username"), 128);
        reply.type = ServerReply::Type::Login;
    } else if(request == "list_characters" && type == "characters") {
        reply.type = ServerReply::Type::Characters;
        reply.hasMore = message.at("has_more").get<bool>();
        const auto& rows = message.at("characters");
        if(!rows.is_array() || rows.size() > 10) throw NetworkError("invalid_response");
        auto previous = command.body.value("after_id", std::int64_t{0});
        for(const auto& row : rows) {
            const auto id = integer(row.at("id"));
            if(id <= previous) throw NetworkError("invalid_response");
            reply.characters.push_back({id, text(row.at("name"), 128)});
            previous = id;
        }
        if(reply.hasMore && reply.characters.empty()) throw NetworkError("invalid_response");
    } else if((request == "select_character" && type == "character_selected") ||
              (request == "enter_world" && type == "world_joined")) {
        const auto& row = message.at("character");
        const auto player = decodePlayer(row);
        CharacterState character;
        character.id = player.id;
        if(request == "select_character" && character.id != command.body.at("character_id").get<std::int64_t>())
            throw NetworkError("invalid_response");
        character.name = player.name;
        character.mapKey = text(row.at("map_key"), 64);
        character.x = player.x;
        character.y = player.y;
        character.facing = player.facing;
        character.level = integer(row.at("level"));
        character.experience = integer(row.at("experience"), 0);
        reply.type = request == "enter_world" ? ServerReply::Type::WorldJoined : ServerReply::Type::CharacterSelected;
        reply.character = std::move(character);
    } else if(request == "leave_world" && type == "world_left") {
        reply.type = ServerReply::Type::WorldLeft;
    } else throw NetworkError("invalid_response");
    return reply;
}

class AsyncGameClient final : public GameClient {
    ServerConfig config;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<Command> commands;
    std::deque<ServerReply> replies;
    RequestId nextId = 1;
    std::atomic<std::uint64_t> generation{0};
    std::atomic<bool> stopping{false};
    std::uint64_t activeGeneration = 0; // Used only by the worker and its TLS callbacks.
    bool authenticated = false;
    std::optional<CharacterState> selected;
    bool inWorld = false;
    std::deque<WorldSnapshot> worldSnapshots;
    std::int64_t latestWorldTick = 0;
    std::optional<std::pair<int, Clock::time_point>> pingSample;
    std::optional<Clock::time_point> pingSent; // Network worker only.
    Clock::time_point nextPing{};
    TlsConnection connection;
    std::thread worker;

    void postWorld(const Json& message) {
        std::lock_guard<std::mutex> lock(mutex);
        if(activeGeneration != generation.load() || stopping || !inWorld) return;
        const auto& rows = message.at("players");
        if(!selected || message.at("map_key") != selected->mapKey || !rows.is_array() ||
           rows.empty() || rows.size() > 10) throw NetworkError("invalid_response");
        WorldSnapshot snapshot;
        snapshot.tick = integer(message.at("tick"));
        if(snapshot.tick <= latestWorldTick) return;
        std::unordered_set<std::int64_t> ids;
        for(const auto& row : rows) {
            auto player = decodePlayer(row);
            if(!ids.insert(player.id).second) throw NetworkError("invalid_response");
            player.moving = row.at("moving").get<bool>();
            player.vx = row.at("vx").get<float>();
            player.vy = row.at("vy").get<float>();
            if(!std::isfinite(player.vx) || !std::isfinite(player.vy)) throw NetworkError("invalid_response");
            player.inputAck = integer(row.at("input_ack"), 0);
            snapshot.players.push_back(std::move(player));
        }
        if(!ids.count(selected->id)) throw NetworkError("invalid_response");
        latestWorldTick = snapshot.tick;
        if(worldSnapshots.size() == 8) worldSnapshots.pop_front();
        worldSnapshots.push_back(std::move(snapshot));
    }

    bool handleEvent(const Json& message) {
        const auto type = message.value("type", "");
        if(type == "pong") {
            if(pingSent) {
                const auto now = Clock::now();
                const auto elapsed = now - *pingSent;
                pingSent.reset();
                std::lock_guard<std::mutex> lock(mutex);
                if(activeGeneration == generation.load() && authenticated && elapsed <= pingFreshness) {
                    const auto milliseconds = std::chrono::round<std::chrono::milliseconds>(elapsed);
                    pingSample = {static_cast<int>(milliseconds.count()), now};
                }
            }
            return true;
        }
        if(type == "move_error") throw NetworkError(text(message.at("code"), 64));
        if(type != "world_state") return false;
        postWorld(message);
        return true;
    }

    ServerReply receiveReply(const Command& command, std::chrono::milliseconds timeout) {
        const auto deadline = Clock::now() + timeout;
        Json header;
        do {
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
            if(remaining.count() <= 0) throw NetworkError("connection_timeout");
            const auto frame = connection.receiveFrame(remaining);
            if(!frame) throw NetworkError("connection_timeout");
            header = Json::parse(*frame);
        } while(handleEvent(header));
        if(command.body.at("type") != "get_map" || header.at("type") == "error")
            return decode(header, command);

        const auto character = selectedCharacter();
        if(header.at("type") != "map_data" || !character || header.at("map_key") != character->mapKey)
            throw NetworkError("invalid_response");
        const auto size = integer(header.at("size"));
        if(size > static_cast<std::int64_t>(maxMapBytes)) throw NetworkError("invalid_response");
        ServerReply reply;
        reply.request = command.id;
        reply.type = ServerReply::Type::MapData;
        reply.mapData.reserve(static_cast<std::size_t>(size));
        while(reply.mapData.size() < static_cast<std::size_t>(size)) {
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
            if(remaining.count() <= 0) throw NetworkError("connection_timeout");
            const auto chunk = connection.receiveFrame(remaining);
            if(!chunk) throw NetworkError("connection_timeout");
            if(chunk->size() != std::min(mapFrameBytes, static_cast<std::size_t>(size) - reply.mapData.size()))
                throw NetworkError("invalid_response");
            reply.mapData += *chunk;
        }
        return reply;
    }

    RequestId enqueue(Json message) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto id = nextId++;
        if(commands.size() >= 16) {
            ServerReply failure; failure.request = id; failure.error = "server_busy";
            replies.push_back(std::move(failure));
        } else commands.push_back({id, generation.load(), std::move(message)});
        wake.notify_one();
        return id;
    }

    void post(ServerReply reply, std::uint64_t version) {
        std::lock_guard<std::mutex> lock(mutex);
        if(version != generation.load() || stopping) return;
        if(reply.type == ServerReply::Type::Login) {
            authenticated = true; selected.reset(); nextPing = Clock::now() + pingInterval;
        }
        if(reply.type == ServerReply::Type::CharacterSelected) selected = reply.character;
        if(reply.type == ServerReply::Type::WorldJoined) {
            if(!selected || !reply.character || selected->id != reply.character->id ||
               selected->mapKey != reply.character->mapKey) throw NetworkError("invalid_response");
            selected = reply.character;
            inWorld = true;
        }
        if(reply.type == ServerReply::Type::WorldLeft) {
            inWorld = false; worldSnapshots.clear(); latestWorldTick = 0;
        }
        if(replies.size() >= 64) replies.pop_front();
        replies.push_back(std::move(reply));
    }

    void clearSession(std::uint64_t version) {
        pingSent.reset();
        std::lock_guard<std::mutex> lock(mutex);
        if(version == generation.load()) {
            authenticated = false; selected.reset(); inWorld = false;
            worldSnapshots.clear(); latestWorldTick = 0;
            pingSample.reset();
        }
    }

    void run() {
        bool connected = false;
        std::uint64_t connectionGeneration = 0;
        while(!stopping) {
            std::optional<Command> command;
            std::chrono::milliseconds idleWait{50};
            {
                std::unique_lock<std::mutex> lock(mutex);
                if(commands.empty() && !connected)
                    wake.wait(lock, [&] { return stopping || !commands.empty(); });
                if(stopping) break;
                if(!commands.empty()) { command = std::move(commands.front()); commands.pop_front(); }
                // Check more often for queued input while the world is active.
                if(inWorld) idleWait = std::chrono::milliseconds(10);
            }
            if(connected && connectionGeneration != generation.load()) {
                connection.close(); connected = false;
            }
            activeGeneration = command ? command->generation : connectionGeneration;
            if(activeGeneration != generation.load()) continue;
            try {
                if(command) {
                    const bool loginRequest = command->body.at("type") == "login";
                    if(loginRequest) {
                        connection.close(); connected = false; clearSession(activeGeneration);
                        connection.connect(config);
                        connected = true; connectionGeneration = activeGeneration;
                    } else if(!connected || !isAuthenticated()) {
                        ServerReply failure; failure.request = command->id; failure.error = "authentication_required";
                        post(std::move(failure), activeGeneration);
                        continue;
                    }
                    const auto timeout = std::chrono::milliseconds(config.timeoutMs);
                    std::string body = command->body.dump();
                    command->body.erase("password");
                    connection.sendFrame(body, timeout);
                    std::fill(body.begin(), body.end(), '\0');
                    if(command->id == 0) continue; // Movement is acknowledged in world_state.
                    auto reply = receiveReply(*command, timeout);
                    if(reply.type == ServerReply::Type::Error && (loginRequest || sessionLost(reply.error))) {
                        connection.close(); connected = false; clearSession(activeGeneration);
                    }
                    post(std::move(reply), activeGeneration);
                } else if(connected) {
                    const auto now = Clock::now();
                    if(!pingSent && now >= nextPing && isAuthenticated()) {
                        pingSent = now;
                        nextPing = now + pingInterval;
                        connection.sendFrame(R"({"type":"ping"})", std::chrono::milliseconds(config.timeoutMs));
                    }
                    const auto frame = connection.receiveFrame(idleWait);
                    if(!frame) continue;
                    const auto message = Json::parse(*frame);
                    if(handleEvent(message)) continue;
                    if(message.value("type", "") != "error") throw NetworkError("invalid_response");
                    ServerReply failure;
                    failure.error = text(message.at("code"), 64);
                    connection.close(); connected = false; clearSession(activeGeneration);
                    post(std::move(failure), activeGeneration);
                }
            } catch(const std::exception& error) {
                connection.close(); connected = false; clearSession(activeGeneration);
                ServerReply failure;
                failure.request = command ? command->id : 0;
                failure.error = dynamic_cast<const NetworkError*>(&error) ? error.what() : "invalid_response";
                post(std::move(failure), activeGeneration);
            }
        }
        connection.close();
    }

public:
    explicit AsyncGameClient(ServerConfig settings) : config(std::move(settings)),
        connection([this] { return stopping.load() || activeGeneration != generation.load(); }),
        worker([this] { run(); }) {}

    ~AsyncGameClient() override {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        connection.interrupt(); wake.notify_all();
        if(worker.joinable()) worker.join();
    }

    RequestId login(std::string username, std::string password) override {
        return enqueue({{"type", "login"}, {"username", std::move(username)}, {"password", std::move(password)}});
    }
    RequestId listCharacters(std::int64_t afterId) override {
        return enqueue({{"type", "list_characters"}, {"after_id", afterId}});
    }
    RequestId selectCharacter(std::int64_t id) override {
        return enqueue({{"type", "select_character"}, {"character_id", id}});
    }
    RequestId downloadMap() override { return enqueue({{"type", "get_map"}}); }
    RequestId enterWorld() override { return enqueue({{"type", "enter_world"}}); }
    bool moveInput(std::int64_t sequence, int dx, int dy) override {
        std::lock_guard<std::mutex> lock(mutex);
        if(!inWorld || commands.size() >= 15) return false;
        commands.push_back({0, generation.load(), {{"type", "move_input"}, {"seq", sequence}, {"dx", dx}, {"dy", dy}}});
        wake.notify_one();
        return true;
    }
    RequestId leaveWorld() override { return enqueue({{"type", "leave_world"}}); }
    std::optional<WorldSnapshot> pollWorld() override {
        std::lock_guard<std::mutex> lock(mutex);
        if(worldSnapshots.empty()) return std::nullopt;
        auto snapshot = std::move(worldSnapshots.front());
        worldSnapshots.pop_front();
        return snapshot;
    }
    std::optional<ServerReply> poll() override {
        std::lock_guard<std::mutex> lock(mutex);
        if(replies.empty()) return std::nullopt;
        auto reply = std::move(replies.front()); replies.pop_front(); return reply;
    }
    bool isAuthenticated() const override {
        std::lock_guard<std::mutex> lock(mutex); return authenticated;
    }
    std::optional<int> pingMs() const override {
        std::lock_guard<std::mutex> lock(mutex);
        if(pingSample && Clock::now() - pingSample->second <= pingFreshness) return pingSample->first;
        return std::nullopt;
    }
    std::optional<CharacterState> selectedCharacter() const override {
        std::lock_guard<std::mutex> lock(mutex); return selected;
    }
    void disconnect() override {
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++generation; commands.clear(); replies.clear(); authenticated = false; selected.reset();
            inWorld = false; worldSnapshots.clear(); latestWorldTick = 0;
            pingSample.reset();
        }
        connection.interrupt(); wake.notify_all();
    }
};
}

std::unique_ptr<GameClient> makeGameClient(ServerConfig config) {
    if(config.host.empty() || config.host.size() > 253 || !config.port || config.caFile.empty() ||
       config.timeoutMs < 1 || config.timeoutMs > 30000) throw std::invalid_argument("Invalid server configuration");
    return std::make_unique<AsyncGameClient>(std::move(config));
}

std::string serverErrorText(const std::string& code) {
    if(code == "invalid_credentials") return u8"Nieprawidłowy login lub hasło.";
    if(code == "rate_limited") return u8"Za dużo prób. Spróbuj ponownie później.";
    if(code == "server_busy") return u8"Serwer jest zajęty. Spróbuj ponownie.";
    if(code == "service_unavailable") return u8"Usługa logowania jest chwilowo niedostępna.";
    if(code == "certificate_missing") return u8"Brak certyfikatu serwera. Sprawdź konfigurację połączenia.";
    if(code == "tls_certificate") return u8"Nie udało się potwierdzić tożsamości serwera.";
    if(code == "tls_failed") return u8"Nie udało się nawiązać bezpiecznego połączenia.";
    if(code == "connection_timeout") return u8"Serwer nie odpowiedział. Spróbuj ponownie.";
    if(code == "connection_closed") return u8"Połączenie z serwerem zostało przerwane.";
    if(code == "connection_failed") return u8"Nie można połączyć się z serwerem.";
    if(sessionLost(code)) return u8"Sesja wygasła. Zaloguj się ponownie.";
    if(code == "character_not_found") return u8"Wybrana postać jest niedostępna. Odśwież listę.";
    if(code == "character_required") return u8"Wybierz postać przed pobraniem mapy.";
    if(code == "map_not_found" || code == "map_unavailable") return u8"Mapa postaci jest niedostępna na serwerze.";
    if(code == "map_required") return u8"Pobierz mapę przed wejściem do świata.";
    if(code == "world_full") return u8"Na tej mapie nie ma wolnych miejsc.";
    if(code == "character_in_world" || code == "already_in_world") return u8"Postać jest już w świecie gry.";
    if(code == "world_required") return u8"Postać nie znajduje się w świecie gry.";
    if(code == "invalid_response") return u8"Serwer zwrócił niepoprawną odpowiedź.";
    return u8"Operacja nie powiodła się. Spróbuj ponownie.";
}
