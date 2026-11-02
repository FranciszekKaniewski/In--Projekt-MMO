#pragma once

#include "../ECS/ECS.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

enum class ChatSource { Player, NPC, Server };

struct ChatMessage {
    ChatSource source = ChatSource::Player;
    ecs::Entity speaker{};
    std::string author;
    std::string text;
};

class ChatSession {
public:
    static constexpr std::size_t maxCharacters = 256;
    static constexpr std::size_t maxHistory = 100;
    using SendHandler = std::function<void(const ChatMessage&)>;

    float bubbleRange = 240.0f;
    float bubbleLifetime = 8.0f;

    void reset(ecs::Entity localSpeaker);
    void setSendHandler(SendHandler handler);
    // Offline mode loops back locally. With a handler, delivery waits for receive().
    bool send(ecs::Registry& world, const std::string& text);
    bool receive(ecs::Registry& world, ChatMessage message);
    void update(ecs::Registry& world, float deltaTime);
    bool canShowBubble(const ecs::Registry& world, ecs::Entity speaker) const;

    ecs::Entity localSpeaker() const { return local; }
    const std::vector<ChatMessage>& messages() const { return history; }
    std::uint64_t revision() const { return historyRevision; }

private:
    ecs::Entity local{};
    std::vector<ChatMessage> history;
    std::uint64_t historyRevision = 0;
    SendHandler sender;
};