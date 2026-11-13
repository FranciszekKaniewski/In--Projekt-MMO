#include "../../Engine/Network/GameClient.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <codecvt>
#include <locale>

namespace {
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
ServerReply await(GameClient& client, RequestId id) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while(std::chrono::steady_clock::now() < end) {
        if(auto reply = client.poll()) {
            require(reply->request == id, "Stale or unexpected reply");
            return *reply;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("Client did not finish its request");
}
}

int wmain(int argc, wchar_t** argv) {
    if(argc != 5) return 2;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utf8;
    const auto mode = utf8.to_bytes(argv[4]);
    try {
        ServerConfig config;
        config.port = static_cast<std::uint16_t>(std::stoi(argv[1]));
        config.host = utf8.to_bytes(argv[2]); config.caFile = utf8.to_bytes(argv[3]);
        if(mode == "timeout") config.timeoutMs = 350;
        auto client = makeGameClient(config);
        const auto start = std::chrono::steady_clock::now();
        auto id = client->login("tester", "Pass123!");
        require(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100), "Login blocks the UI");
        if(mode == "cancel") {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            client->disconnect();
            id = client->login("tester", "Pass123!");
        }
        const auto login = await(*client, id);
        if(mode == "hostname" || mode == "untrusted" || mode == "invalid_credentials" ||
           mode == "oversized" || mode == "timeout" || mode == "missing_ca") {
            const std::string expected = mode == "hostname" || mode == "untrusted" ? "tls_certificate" :
                mode == "oversized" ? "invalid_response" : mode == "timeout" ? "connection_timeout" :
                mode == "missing_ca" ? "certificate_missing" : "invalid_credentials";
            require(login.type == ServerReply::Type::Error, "Expected a rejected login");
            if(login.error != expected) throw std::runtime_error("Expected " + expected + ", got " + login.error);
            require(!client->isAuthenticated(), "Failed login authenticated the connection");
        } else {
            if(login.type != ServerReply::Type::Login) throw std::runtime_error("Login failed: " + login.error);
            require(client->isAuthenticated(), "Login did not create a session");
            const auto list = await(*client, client->listCharacters());
            require(list.type == ServerReply::Type::Characters && list.characters.size() == 1, "Incorrect character list");
            require(list.characters[0].id == 9007199254741009LL, "Character ID lost precision");
            require(list.characters[0].name == u8"Żółw", "Unicode name was changed");
            const auto selection = await(*client, client->selectCharacter(list.characters[0].id));
            require(selection.type == ServerReply::Type::CharacterSelected && selection.character.has_value(), "Selection failed");
            require(client->selectedCharacter()->x == 1350 && client->selectedCharacter()->facing == 4, "Character state was not retained");
            if(mode == "map" || mode == "map_wrong_key" || mode == "map_truncated") {
                const auto downloaded = await(*client, client->downloadMap());
                if(mode == "map") {
                    std::ifstream map("../server/priv/maps/Map_expanded.tmj", std::ios::binary);
                    require(map.good(), "Missing map fixture");
                    const std::string expected((std::istreambuf_iterator<char>(map)), {});
                    require(downloaded.type == ServerReply::Type::MapData && downloaded.mapData == expected,
                            "Map transfer changed the file contents");
                    require(client->isAuthenticated(), "Map download closed the session");
                } else {
                    const std::string expected = mode == "map_wrong_key" ? "invalid_response" : "connection_closed";
                    require(downloaded.type == ServerReply::Type::Error && downloaded.error == expected,
                            "Bad map transfer was accepted");
                }
            }
            if(mode == "expired") {
                const auto expired = await(*client, 0);
                require(expired.error == "session_expired" && !client->isAuthenticated(), "Expiration did not revoke the session");
                require(!client->selectedCharacter(), "Expiration retained a selected character");
            }
            if(mode == "world") {
                const auto joined = await(*client, client->enterWorld());
                require(joined.type == ServerReply::Type::WorldJoined && joined.character->x == 1352,
                        "Server spawn was not applied");
                require(await(*client, client->moveInput(1, 0)).type == ServerReply::Type::Move,
                        "World event interrupted the movement response");
                const auto state = client->pollWorld();
                require(state && state->size() == 2 && state->front().id == 9007199254741009LL &&
                        state->front().x == 1368 && state->front().moving && state->back().name == "Peer",
                        "The latest world snapshot was not delivered");
                require(await(*client, client->leaveWorld()).type == ServerReply::Type::WorldLeft,
                        "Leave world failed");
                require(!client->pollWorld() && client->isAuthenticated(), "Leaving retained stale world state");
                require(await(*client, client->listCharacters()).type == ServerReply::Type::Characters,
                        "The TLS session did not survive leaving the world");
            }
        }
        client->disconnect();
        require(!client->isAuthenticated() && !client->selectedCharacter(), "Disconnect retained session state");
        std::cout << mode << ": passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << mode << ": " << error.what() << '\n';
        return 1;
    }
}
