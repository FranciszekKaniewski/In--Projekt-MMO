#include "Chat.h"

#include "../ECS/Components/ChatSpeakerComponent.h"
#include "../ECS/Components/SpeechBubbleComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../Text/Utf8.h"

#include <cmath>
#include <utility>

namespace {
std::string cleanText(const std::string& value, std::size_t limit) {
    auto result = utf8::singleLine(value, limit);
    const auto start = result.find_first_not_of(' ');
    if (start == std::string::npos) return {};
    return result.substr(start, result.find_last_not_of(' ') - start + 1);
}
}

void ChatSession::reset(ecs::Entity localSpeaker) {
    local = localSpeaker;
    history.clear();
    sender = {};
    ++historyRevision;
}

void ChatSession::setSendHandler(SendHandler handler) { sender = std::move(handler); }

bool ChatSession::send(ecs::Registry& world, const std::string& text) {
    const auto* speaker = world.tryGetComponent<ChatSpeakerComponent>(local);
    if (!speaker) return false;
    ChatMessage message{speaker->npc ? ChatSource::NPC : ChatSource::Player,
                        local, speaker->name, cleanText(text, maxCharacters)};
    if (message.text.empty()) return false;
    if (sender) sender(message);
    else return receive(world, std::move(message));
    return true;
}

bool ChatSession::receive(ecs::Registry& world, ChatMessage message) {
    message.text = cleanText(message.text, maxCharacters);
    if (message.text.empty()) return false;
    if (message.source == ChatSource::Server) {
        message.speaker = {};
        message.author = "Serwer";
    } else {
        if (message.source != ChatSource::Player && message.source != ChatSource::NPC) return false;
        const auto* speaker = world.tryGetComponent<ChatSpeakerComponent>(message.speaker);
        if (speaker) {
            message.author = speaker->name;
            message.source = speaker->npc ? ChatSource::NPC : ChatSource::Player;
            if (world.hasComponent<TransformComponent>(message.speaker)) {
                auto* bubble = world.tryGetComponent<SpeechBubbleComponent>(message.speaker);
                if (!bubble) bubble = &world.addComponent<SpeechBubbleComponent>(message.speaker);
                bubble->text = message.text;
                bubble->remaining = bubbleLifetime;
            }
        }
        message.author = cleanText(message.author, 32);
        if (message.author.empty()) message.author = message.source == ChatSource::NPC ? "NPC" : "Gracz";
    }
    if (history.size() == maxHistory) history.erase(history.begin());
    history.push_back(std::move(message));
    ++historyRevision;
    return true;
}

void ChatSession::update(ecs::Registry& world, float deltaTime) {
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f) return;
    world.forEach<SpeechBubbleComponent>([&world, deltaTime](ecs::Entity entity, SpeechBubbleComponent& bubble) {
        bubble.remaining -= deltaTime;
        if (!std::isfinite(bubble.remaining) || bubble.remaining <= 0.0f)
            world.removeComponent<SpeechBubbleComponent>(entity);
    });
}

bool ChatSession::canShowBubble(const ecs::Registry& world, ecs::Entity speaker) const {
    if (!std::isfinite(bubbleRange) || bubbleRange < 0.0f) return false;
    const auto* listenerPosition = world.tryGetComponent<TransformComponent>(local);
    const auto* speakerPosition = world.tryGetComponent<TransformComponent>(speaker);
    if (!listenerPosition || !speakerPosition) return false;
    const double dx = static_cast<double>(speakerPosition->position.x) - listenerPosition->position.x;
    const double dy = static_cast<double>(speakerPosition->position.y) - listenerPosition->position.y;
    return std::hypot(dx, dy) <= bubbleRange;
}
