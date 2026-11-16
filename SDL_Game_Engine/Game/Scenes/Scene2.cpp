#include "../../Engine/Scenes/GameScene.h"
#include "../../Engine/ECS/Components/KeyboardControlComponent.h"
#include "../../Engine/ECS/Components/MovementBoundsComponent.h"
#include "../../Engine/ECS/Components/MovementComponent.h"
#include "../../Engine/ECS/Components/MovementInputComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/ECS/Components/VelocityComponent.h"
#include "../../Engine/ECS/Components/ChatSpeakerComponent.h"
#include "../../Engine/Scenes/SceneManager.h"
#include "../../Engine/World/Map.h"
#include "../Entities/Player.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>

namespace {
constexpr double interpolationDelay = 0.045;
constexpr float walkAnimationGrace = static_cast<float>(2 * worldStep);
constexpr float correctionDuration = 0.05f;
constexpr float maxSmoothCorrection = 12.0f;
constexpr std::size_t maxPendingInputs = 16;
constexpr float movementSpeed = 160.0f;

struct RemoteSample {
    std::int64_t tick;
    math::Vector2 position;
    math::Vector2 velocity;
    FacingDirection facing;
    bool moving;
};

struct RemoteMotion {
    std::deque<RemoteSample> samples;
    float walkRemaining = 0.0f;
};

struct MovementInput {
    std::int64_t sequence;
    math::Vector2 direction;
};
}

class Scene2 : public GameScene {
private:
    Map map;
    CharacterState saved;
    ecs::Entity player;
    std::unordered_map<std::int64_t, ecs::Entity> others;
    std::deque<MovementInput> pendingInputs;
    std::int64_t nextSequence = 1;
    std::int64_t latestTick = 0;
    double renderTick = 0;
    math::Vector2 predicted;
    math::Vector2 correction;
    FacingDirection predictedFacing = FacingDirection::South;
    bool sentMovement = false;
    RequestId pendingLeave = 0;

    math::Vector2 clampToMap(math::Vector2 position) const {
        const auto size = map.worldSize();
        return {std::clamp(position.x, 10.0f, size.x - 10.0f),
                std::clamp(position.y, 5.0f, size.y - 5.0f)};
    }

    void predict(math::Vector2 direction) {
        if(direction.lengthSquared() > 0) {
            predicted = clampToMap(predicted + direction.normalized() * (movementSpeed * movementStep));
            predictedFacing = movementFacing(direction);
        }
    }

    void reconcile(const WorldPlayer& state) {
        const auto displayed = predicted + correction;
        while(!pendingInputs.empty() && pendingInputs.front().sequence <= state.inputAck)
            pendingInputs.pop_front();
        predicted = {state.x, state.y};
        predictedFacing = static_cast<FacingDirection>(state.facing);
        for(const auto& input : pendingInputs) predict(input.direction);
        correction = displayed - predicted;
        if(correction.lengthSquared() > maxSmoothCorrection * maxSmoothCorrection) correction = {};
    }

    void applySnapshot(const WorldSnapshot& snapshot) {
        if(snapshot.tick <= latestTick) return;
        latestTick = snapshot.tick;
        const auto desiredTick = latestTick - interpolationDelay / worldStep;
        renderTick = std::max(renderTick, desiredTick);
        for(auto it = others.begin(); it != others.end();) {
            const auto id = it->first;
            if(std::none_of(snapshot.players.begin(), snapshot.players.end(),
                            [id](const auto& state) { return state.id == id; })) {
                world.destroyEntity(it->second);
                it = others.erase(it);
            } else {
                ++it;
            }
        }
        for(const auto& state : snapshot.players) {
            if(state.id == saved.id) {
                reconcile(state);
                continue;
            }
            auto found = others.find(state.id);
            if(found == others.end()) {
                const auto entity = spawnPlayer(world, {state.x, state.y});
                if(!entity) continue;
                world.removeComponent<VelocityComponent>(entity);
                world.addComponent<RemoteMotion>(entity);
                world.getComponent<ChatSpeakerComponent>(entity).name = state.name;
                found = others.emplace(state.id, entity).first;
            }
            auto& samples = world.getComponent<RemoteMotion>(found->second).samples;
            if(samples.size() == 4) samples.pop_front();
            samples.push_back({snapshot.tick, {state.x, state.y}, {state.vx, state.vy},
                               static_cast<FacingDirection>(state.facing), state.moving});
        }
    }

    void updateRemotePlayers(float elapsed) {
        if(!latestTick) return;
        renderTick = std::min(renderTick + elapsed / worldStep, static_cast<double>(latestTick) + 1.0);
        for(const auto& entry : others) {
            const auto entity = entry.second;
            auto& remote = world.getComponent<RemoteMotion>(entity);
            auto& samples = remote.samples;
            while(samples.size() > 1 && samples[1].tick <= renderTick) samples.pop_front();
            const auto& first = samples.front();
            auto position = first.position;
            bool moving = first.moving;
            if(samples.size() > 1) {
                const auto& second = samples[1];
                const auto fraction = static_cast<float>(std::clamp(
                    (renderTick - first.tick) / (second.tick - first.tick), 0.0, 1.0));
                position += (second.position - first.position) * fraction;
                moving = (second.position - first.position).lengthSquared() > 0;
            } else {
                const auto ahead = std::clamp(renderTick - first.tick, 0.0, 1.0);
                position += first.velocity * static_cast<float>(ahead * worldStep);
                moving = moving && ahead < 1.0;
            }
            world.getComponent<TransformComponent>(entity).position = clampToMap(position);
            auto& movement = world.getComponent<MovementComponent>(entity);
            // Keep the walk clip playing through a brief gap in received movement.
            remote.walkRemaining = moving ? walkAnimationGrace : std::max(0.0f, remote.walkRemaining - elapsed);
            movement.facing = first.facing;
            movement.moving = remote.walkRemaining > 0.0f;
        }
    }

public:
    Scene2(Map loaded, CharacterState character) : map(std::move(loaded)), saved(std::move(character)) {}

    void onEnter(App& app) override {
        std::cout << "Entering Game Scene !" << std::endl;

        const auto screen = app.getUISize();
        const float width = static_cast<float>(screen.wight);
        map.scale = 1.0f;
        map.position = {};
        const auto mapSize = map.worldSize();

        world.clear();

        player = spawnPlayer(world, {saved.x, saved.y});
        if (!player) return;
        world.getComponent<ChatSpeakerComponent>(player).name = saved.name;
        world.getComponent<MovementComponent>(player).facing = static_cast<FacingDirection>(saved.facing);
        world.addComponent<KeyboardControlComponent>(player);
        if(saved.id) {
            predicted = {saved.x, saved.y};
            predictedFacing = static_cast<FacingDirection>(saved.facing);
            world.removeComponent<VelocityComponent>(player);
        } else {
            auto& bounds = world.addComponent<MovementBoundsComponent>(player);
            bounds.minimum = map.position + math::Vector2{10.0f, 5.0f};
            bounds.maximum = map.position + mapSize - math::Vector2{10.0f, 5.0f};
        }

        // Camera and character belong to the same entity; this scene only composes them.
        cameraEntity = player;
        auto& view = world.addComponent<CameraComponent>(cameraEntity);
        view.size = {width, static_cast<float>(screen.height)};
        view.target = player;
        view.following = true;
        view.bounded = true;
        view.bounds = {map.position.x, map.position.y, mapSize.x, mapSize.y};
        updateCameras(world);
        enableChat(player);
        if(saved.id && app.serverClient) {
            chatSession().bubbleRange.reset();
            chatSession().setSendHandler([this, &app](const ChatMessage& message) {
                if(!pendingLeave && !app.serverClient->sendChat(message.text))
                    receiveServerMessage(serverErrorText("server_busy"));
            });
        }
    }

protected:
    void updateWorld(App& app) override {
        if(!saved.id || !app.serverClient || !player) return;
        while(auto reply = app.serverClient->poll()) {
            if(pendingLeave && reply->request == pendingLeave &&
               reply->type == ServerReply::Type::WorldLeft) {
                App::sceneManager->changeScene(createCharactersScene, app);
                return;
            }
            if(reply->type == ServerReply::Type::ChatError) {
                receiveServerMessage(reply->error == "invalid_message" ?
                    u8"Serwer odrzucił wiadomość czatu." : serverErrorText(reply->error));
            }
            if(reply->type == ServerReply::Type::Error) {
                if(!app.serverClient->isAuthenticated()) {
                    app.loginMessage = serverErrorText(reply->error);
                    App::sceneManager->changeScene(createScene1, app);
                    return;
                }
                if(reply->error == "world_required") {
                    App::sceneManager->changeScene(createCharactersScene, app);
                    return;
                }
                if(pendingLeave && reply->request == pendingLeave) {
                    pendingLeave = 0;
                    receiveServerMessage(serverErrorText(reply->error));
                }
            }
        }
        while(auto snapshot = app.serverClient->pollWorld()) applySnapshot(*snapshot);
        while(auto message = app.serverClient->pollChat()) {
            ecs::Entity speaker;
            if(message->characterId == saved.id) speaker = player;
            else if(const auto found = others.find(message->characterId); found != others.end())
                speaker = found->second;
            receiveChatMessage({ChatSource::Player, speaker, std::move(message->author), std::move(message->text)});
        }
        const float elapsed = App::deltaTime;
        if(!std::isfinite(elapsed) || elapsed < 0) return;
        updateRemotePlayers(elapsed);
        const auto direction = world.getComponent<MovementInputComponent>(player).direction;
        const bool wantsToMove = direction.lengthSquared() > 0;
        const auto before = predicted;
        if(!pendingLeave && elapsed > 0 && (wantsToMove || sentMovement) &&
           pendingInputs.size() < maxPendingInputs &&
           app.serverClient->moveInput(nextSequence, static_cast<int>(direction.x), static_cast<int>(direction.y))) {
            pendingInputs.push_back({nextSequence++, direction});
            predict(direction);
            sentMovement = wantsToMove;
        }
        correction *= std::max(0.0f, 1.0f - elapsed / correctionDuration);
        world.getComponent<TransformComponent>(player).position = clampToMap(predicted + correction);
        auto& movement = world.getComponent<MovementComponent>(player);
        movement.facing = predictedFacing;
        movement.moving = (predicted - before).lengthSquared() > 0;
    }

    void renderWorld(App& app) override {
        map.draw(app.renderer, camera());
    }

    void handleWorldEvents(App& app, SDL_Event &event) override {
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE && !event.key.repeat) {
                const bool authenticated = app.serverClient && app.serverClient->isAuthenticated();
                if(saved.id && authenticated) {
                    if(!pendingLeave) pendingLeave = app.serverClient->leaveWorld();
                    return;
                }
                app.sceneManager->changeScene(authenticated ? createCharactersScene : createScene1, app);
            }
        }
    }
};

Scene* createNetworkGameScene(Map map, CharacterState character) {
    return new Scene2(std::move(map), std::move(character));
}
