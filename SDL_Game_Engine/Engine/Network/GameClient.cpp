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

namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;

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
    } else if(request == "select_character" && type == "character_selected") {
        const auto& row = message.at("character");
        CharacterState character;
        character.id = integer(row.at("id"));
        if(character.id != command.body.at("character_id").get<std::int64_t>())
            throw NetworkError("invalid_response");
        character.name = text(row.at("name"), 128);
        character.mapKey = text(row.at("map_key"), 64);
        character.x = row.at("position_x").get<float>();
        character.y = row.at("position_y").get<float>();
        const auto facing = integer(row.at("facing"), 0);
        if(!std::isfinite(character.x) || !std::isfinite(character.y) || facing > 7)
            throw NetworkError("invalid_response");
        character.facing = static_cast<int>(facing);
        character.level = integer(row.at("level"));
        character.experience = integer(row.at("experience"), 0);
        reply.type = ServerReply::Type::CharacterSelected;
        reply.character = std::move(character);
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
    TlsConnection connection;
    std::thread worker;

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
        if(reply.type == ServerReply::Type::Login) { authenticated = true; selected.reset(); }
        if(reply.type == ServerReply::Type::CharacterSelected) selected = reply.character;
        if(replies.size() >= 64) replies.pop_front();
        replies.push_back(std::move(reply));
    }

    void clearSession(std::uint64_t version) {
        std::lock_guard<std::mutex> lock(mutex);
        if(version == generation.load()) { authenticated = false; selected.reset(); }
    }

    void run() {
        bool connected = false;
        std::uint64_t connectionGeneration = 0;
        while(!stopping) {
            std::optional<Command> command;
            {
                std::unique_lock<std::mutex> lock(mutex);
                if(commands.empty() && !connected)
                    wake.wait(lock, [&] { return stopping || !commands.empty(); });
                if(stopping) break;
                if(!commands.empty()) { command = std::move(commands.front()); commands.pop_front(); }
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
                    const auto frame = connection.receiveFrame(timeout);
                    if(!frame) throw NetworkError("connection_timeout");
                    auto reply = decode(Json::parse(*frame), *command);
                    if(reply.type == ServerReply::Type::Error && (loginRequest || sessionLost(reply.error))) {
                        connection.close(); connected = false; clearSession(activeGeneration);
                    }
                    post(std::move(reply), activeGeneration);
                } else if(connected) {
                    const auto frame = connection.receiveFrame(std::chrono::milliseconds(50));
                    if(!frame) continue;
                    const auto message = Json::parse(*frame);
                    if(message.value("type", "") == "pong") continue;
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
    std::optional<ServerReply> poll() override {
        std::lock_guard<std::mutex> lock(mutex);
        if(replies.empty()) return std::nullopt;
        auto reply = std::move(replies.front()); replies.pop_front(); return reply;
    }
    bool isAuthenticated() const override {
        std::lock_guard<std::mutex> lock(mutex); return authenticated;
    }
    std::optional<CharacterState> selectedCharacter() const override {
        std::lock_guard<std::mutex> lock(mutex); return selected;
    }
    void disconnect() override {
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++generation; commands.clear(); replies.clear(); authenticated = false; selected.reset();
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
    if(code == "invalid_response") return u8"Serwer zwrócił niepoprawną odpowiedź.";
    return u8"Operacja nie powiodła się. Spróbuj ponownie.";
}
