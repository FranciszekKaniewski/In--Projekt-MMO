#include <catch_amalgamated.hpp>
#define SDL_MAIN_HANDLED
#include "../../Engine/Chat/Chat.h"
#include "../../Engine/ECS/Components/ChatSpeakerComponent.h"
#include "../../Engine/ECS/Components/KeyboardControlComponent.h"
#include "../../Engine/ECS/Components/MovementComponent.h"
#include "../../Engine/ECS/Components/MovementInputComponent.h"
#include "../../Engine/ECS/Components/SpeechBubbleComponent.h"
#include "../../Engine/ECS/Components/TransformComponent.h"
#include "../../Engine/ECS/Components/VelocityComponent.h"
#include "../../Engine/Scenes/GameScene.h"
#include "../../Engine/Scenes/SceneList.h"
#include "../../Engine/Text/Utf8.h"
#include "../../Game/Entities/Player.h"

#include <SDL_image.h>
#include <array>
#include <limits>
#include <memory>

namespace {
ecs::Entity addSpeaker(ecs::Registry& world, math::Vector2 position, const std::string& name, bool npc = false) {
    const auto entity = world.createEntity();
    world.addComponent<TransformComponent>(entity, position);
    auto& speaker = world.addComponent<ChatSpeakerComponent>(entity);
    speaker.name = name;
    speaker.npc = npc;
    return entity;
}

TEST_CASE("Chat routes local players, received NPC messages and server announcements", "[integration][chat]") {
    ecs::Registry world;
    const auto local = addSpeaker(world, {0, 0}, "Ala");
    const auto npc = addSpeaker(world, {100, 0}, u8"Strażnik", true);
    ChatSession chat;
    chat.reset(local);
    REQUIRE(chat.send(world, u8"  Cześć!  "));
    REQUIRE(chat.messages().back().text == u8"Cześć!");
    REQUIRE(chat.messages().back().author == "Ala");
    REQUIRE(world.getComponent<SpeechBubbleComponent>(local).text == u8"Cześć!");
    REQUIRE(chat.canShowBubble(world, local));
    REQUIRE(chat.receive(world, {ChatSource::NPC, npc, "wrong name", u8"Witaj podróżniku!"}));
    REQUIRE(chat.messages().back().author == u8"Strażnik");
    REQUIRE(chat.messages().back().source == ChatSource::NPC);
    REQUIRE(world.hasComponent<SpeechBubbleComponent>(npc));
    REQUIRE(chat.receive(world, {ChatSource::Server, npc, "wrong name", "Restart za 5 minut"}));
    REQUIRE(chat.messages().back().author == "Serwer");
    REQUIRE_FALSE(chat.messages().back().speaker);
    REQUIRE(world.getComponent<SpeechBubbleComponent>(npc).text == u8"Witaj podróżniku!");

    ChatMessage outgoing;
    chat.setSendHandler([&](const ChatMessage& message) { outgoing = message; });
    const auto count = chat.messages().size();
    REQUIRE(chat.send(world, "Transport"));
    REQUIRE(outgoing.text == "Transport");
    REQUIRE(outgoing.speaker == local);
    REQUIRE(chat.messages().size() == count); // Network mode waits for the authoritative echo.
    REQUIRE(chat.receive(world, outgoing));
    REQUIRE(chat.messages().size() == count + 1);
}

TEST_CASE("Chat limits incoming and outgoing Unicode messages and retains bounded history", "[integration][chat]") {
    ecs::Registry world;
    const auto local = addSpeaker(world, {}, "Ala");
    ChatSession chat;
    chat.reset(local);
    std::string longMessage;
    for (int i = 0; i < 300; ++i) longMessage += u8"ż";
    REQUIRE(chat.send(world, longMessage));
    REQUIRE(utf8::count(chat.messages().back().text) == 256);
    REQUIRE(chat.messages().back().text.size() == 512);
    REQUIRE(chat.receive(world, {ChatSource::Player, {}, "Remote", longMessage}));
    REQUIRE(utf8::count(chat.messages().back().text) == 256);
    REQUIRE_FALSE(chat.send(world, " \t\n "));
    REQUIRE_FALSE(chat.receive(world, {ChatSource::Server, {}, "", ""}));
    for (int i = 0; i < 110; ++i)
        REQUIRE(chat.receive(world, {ChatSource::Server, {}, "", std::to_string(i)}));
    REQUIRE(chat.messages().size() == ChatSession::maxHistory);
    REQUIRE(chat.messages().front().text == "10");
    REQUIRE(chat.messages().back().text == "109");
}

TEST_CASE("Speech bubbles follow proximity, refresh, expire and survive deleted speakers", "[integration][chat]") {
    ecs::Registry world;
    const auto local = addSpeaker(world, {}, "Ala");
    const auto near = addSpeaker(world, {240, 0}, "Bob");
    const auto far = addSpeaker(world, {241, 0}, "NPC", true);
    ChatSession chat;
    chat.reset(local);
    REQUIRE(chat.receive(world, {ChatSource::Player, near, "", "Blisko"}));
    REQUIRE(chat.receive(world, {ChatSource::NPC, far, "", "Daleko"}));
    REQUIRE(chat.canShowBubble(world, near));
    REQUIRE_FALSE(chat.canShowBubble(world, far));
    world.getComponent<TransformComponent>(local).position.x = 1;
    REQUIRE(chat.canShowBubble(world, far));
    chat.update(world, 7.0f);
    REQUIRE(world.hasComponent<SpeechBubbleComponent>(near));
    REQUIRE(chat.receive(world, {ChatSource::Player, near, "", "Nowa wiadomość"}));
    chat.update(world, 1.0f);
    REQUIRE_FALSE(world.hasComponent<SpeechBubbleComponent>(far));
    REQUIRE(world.getComponent<SpeechBubbleComponent>(near).remaining == 7.0f);
    chat.update(world, std::numeric_limits<float>::quiet_NaN());
    REQUIRE(world.getComponent<SpeechBubbleComponent>(near).remaining == 7.0f);
    chat.update(world, 7.0f);
    REQUIRE_FALSE(world.hasComponent<SpeechBubbleComponent>(near));
    world.destroyEntity(near);
    REQUIRE(chat.receive(world, {ChatSource::Player, near, "Bob", "Delayed message"}));
    REQUIRE_FALSE(world.hasComponent<SpeechBubbleComponent>(near));
    REQUIRE_FALSE(chat.canShowBubble(world, near));
}

struct ChatSDLFixture {
    ChatSDLFixture() {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
        REQUIRE(TTF_Init() == 0);
        REQUIRE((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != 0);
    }
    ~ChatSDLFixture() { TTF_Quit(); IMG_Quit(); SDL_Quit(); }
};

class ChatCanvas {
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface{nullptr, SDL_FreeSurface};
    const float previousDeltaTime = App::deltaTime;
public:
    App app;
    ChatCanvas() {
        REQUIRE(App::renderer == nullptr);
        surface.reset(SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA32));
        REQUIRE(surface != nullptr);
        App::renderer = SDL_CreateSoftwareRenderer(surface.get());
        REQUIRE(App::renderer != nullptr);
        REQUIRE(SDL_RenderSetLogicalSize(App::renderer, 1280, 720) == 0);
    }
    ~ChatCanvas() { App::deltaTime = previousDeltaTime; }
    int lightPixels(SDL_Rect area) const {
        std::vector<Uint8> pixels(area.w * area.h * 4);
        REQUIRE(SDL_RenderReadPixels(App::renderer, &area, SDL_PIXELFORMAT_RGBA32, pixels.data(), area.w * 4) == 0);
        int count = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 4)
            if (pixels[i] > 235 && pixels[i + 1] > 235 && pixels[i + 2] > 235) ++count;
        return count;
    }
    std::uint64_t checksum(SDL_Rect area) const {
        std::vector<Uint8> pixels(area.w * area.h * 4);
        REQUIRE(SDL_RenderReadPixels(App::renderer, &area, SDL_PIXELFORMAT_RGBA32, pixels.data(), area.w * 4) == 0);
        std::uint64_t result = 14695981039346656037ULL;
        for (auto byte : pixels) result = (result ^ byte) * 1099511628211ULL;
        return result;
    }
    void savePreview(const char* path) const {
        SDL_RenderPresent(App::renderer);
        REQUIRE(SDL_SaveBMP(surface.get(), path) == 0);
    }
};

SDL_Event key(SDL_Keycode code, Uint16 mod = 0) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = code;
    event.key.keysym.mod = mod;
    return event;
}
SDL_Event text(const char* value) {
    SDL_Event event{};
    event.type = SDL_TEXTINPUT;
    SDL_strlcpy(event.text.text, value, sizeof(event.text.text));
    return event;
}

TEST_CASE_METHOD(ChatSDLFixture, "Text input enforces 256 characters on typing, paste and UTF-8 edits",
                 "[integration][chat][ui]") {
    ChatCanvas canvas;
    Input input(Font("Assets/fonts/Lato-Regular.ttf", 18), "", {20, 20, 400, 36});
    input.setMaxLength(256);
    input.setFocused(true);
    for (int i = 0; i < 260; ++i) REQUIRE(input.handleEvent(text(u8"ą")));
    REQUIRE(input.getLength() == 256);
    REQUIRE(input.getText().size() == 512);
    REQUIRE(input.handleEvent(key(SDLK_BACKSPACE)));
    REQUIRE(input.getLength() == 255);
    REQUIRE(input.handleEvent(text(u8"😀")));
    REQUIRE(input.getLength() == 256);
    REQUIRE(input.getText().substr(input.getText().size() - 4) == u8"😀");
    REQUIRE(input.handleEvent(key(SDLK_HOME)));
    REQUIRE(input.handleEvent(key(SDLK_DELETE)));
    REQUIRE(input.getLength() == 255);
    REQUIRE(input.handleEvent(text(u8"ż")));
    REQUIRE(input.getText().substr(0, 2) == u8"ż");
    input.setText("abc");
    REQUIRE(SDL_SetClipboardText(std::string(400, 'x').c_str()) == 0);
    REQUIRE(input.handleEvent(key(SDLK_v, KMOD_CTRL)));
    REQUIRE(input.getLength() == 256);
    REQUIRE(input.getText().substr(0, 3) == "abc");
    input.draw();
}

class ChatTestScene : public GameScene {
public:
    ecs::Entity local, nearby, distant;
    int worldEvents = 0;
    ChatTestScene() {
        local = spawnPlayer(world, {640, 360});
        nearby = spawnPlayer(world, {800, 360});
        distant = spawnPlayer(world, {1040, 360});
        REQUIRE(local);
        REQUIRE(nearby);
        REQUIRE(distant);
        world.addComponent<KeyboardControlComponent>(local);
        world.getComponent<ChatSpeakerComponent>(local).name = "Ty";
        auto& npc = world.getComponent<ChatSpeakerComponent>(nearby);
        npc.name = u8"Strażnik";
        npc.npc = true;
        cameraEntity = local;
        auto& view = world.addComponent<CameraComponent>(local);
        view.target = local;
        view.following = true;
        updateCameras(world);
        enableChat(local);
    }
    bool focused() const { return chatOverlay->isFocused(); }
    const std::string& draft() const { return chatOverlay->draft(); }
    bool hasBubble(ecs::Entity entity) const { return world.hasComponent<SpeechBubbleComponent>(entity); }
    void command(ecs::Entity entity, math::Vector2 direction) { world.getComponent<MovementInputComponent>(entity).direction = direction; }
    math::Vector2 position(ecs::Entity entity) const { return world.getComponent<TransformComponent>(entity).position; }
    void setPosition(ecs::Entity entity, math::Vector2 position) { world.getComponent<TransformComponent>(entity).position = position; }
protected:
    void handleWorldEvents(App&, SDL_Event&) override { ++worldEvents; }
};

TEST_CASE_METHOD(ChatSDLFixture, "Chat captures text and Escape while remote movement continues",
                 "[integration][chat][ui]") {
    ChatCanvas canvas;
    ChatTestScene scene;
    auto event = key(SDLK_RETURN);
    scene.handleEvents(canvas.app, event);
    REQUIRE(scene.focused());
    event = text(u8"Cześć świecie!");
    scene.handleEvents(canvas.app, event);
    REQUIRE(scene.draft() == u8"Cześć świecie!");
    REQUIRE(scene.worldEvents == 0);
    scene.command(scene.local, {1, 0});
    scene.command(scene.nearby, {1, 0});
    App::deltaTime = 0.125f;
    scene.update(canvas.app);
    REQUIRE(scene.position(scene.local).x == 640.0f);
    REQUIRE(scene.position(scene.nearby).x == 820.0f);
    event = key(SDLK_RETURN);
    scene.handleEvents(canvas.app, event);
    REQUIRE_FALSE(scene.focused());
    REQUIRE(scene.draft().empty());
    REQUIRE(scene.chatSession().messages().back().text == u8"Cześć świecie!");
    REQUIRE(scene.hasBubble(scene.local));
    scene.handleEvents(canvas.app, event);
    event = key(SDLK_ESCAPE);
    scene.handleEvents(canvas.app, event);
    REQUIRE_FALSE(scene.focused());
    REQUIRE(scene.worldEvents == 0);
    scene.handleEvents(canvas.app, event);
    REQUIRE(scene.worldEvents == 1);
    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 40;
    event.button.y = 680;
    scene.handleEvents(canvas.app, event);
    REQUIRE(scene.focused());
    event = {};
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    scene.handleEvents(canvas.app, event);
    REQUIRE_FALSE(scene.focused());
}

TEST_CASE_METHOD(ChatSDLFixture, "Speech bubbles render over self and nearby NPC and follow the camera",
                 "[integration][chat][ui]") {
    ChatCanvas canvas;
    ChatTestScene scene;
    REQUIRE(scene.receiveChatMessage({ChatSource::Player, scene.local, "", u8"Cześć!"}));
    REQUIRE(scene.receiveChatMessage({ChatSource::NPC, scene.nearby, "", "Witaj!"}));
    REQUIRE(scene.receiveChatMessage({ChatSource::Player, scene.distant, "", "Daleko"}));
    scene.render(canvas.app);
    REQUIRE(canvas.lightPixels({580, 220, 120, 70}) > 500);
    REQUIRE(canvas.lightPixels({740, 220, 120, 70}) > 500);
    REQUIRE(canvas.lightPixels({980, 220, 120, 70}) == 0);
    scene.setPosition(scene.local, {740, 360});
    App::deltaTime = 0.0f;
    scene.update(canvas.app);
    scene.render(canvas.app);
    REQUIRE(canvas.lightPixels({580, 220, 120, 70}) > 500);
    REQUIRE(canvas.lightPixels({680, 220, 120, 70}) > 500);
    App::deltaTime = 9.0f;
    scene.update(canvas.app);
    scene.render(canvas.app);
    REQUIRE(canvas.lightPixels({580, 220, 120, 70}) == 0);
    REQUIRE(canvas.lightPixels({680, 220, 120, 70}) == 0);
}

TEST_CASE_METHOD(ChatSDLFixture, "Game chat renders a wrapped 256-character bubble and bottom-left panel",
                 "[integration][chat][ui]") {
    ChatCanvas canvas;
    std::unique_ptr<Scene> scene(createScene2());
    scene->onEnter(canvas.app);
    auto* game = dynamic_cast<GameScene*>(scene.get());
    REQUIRE(game != nullptr);
    REQUIRE(game->receiveChatMessage({ChatSource::Player, game->chatSession().localSpeaker(), "", std::string(256, 'W')}));
    scene->render(canvas.app);
    REQUIRE(canvas.lightPixels({490, 8, 300, 280}) > 500);
    canvas.savePreview("chat-long-message-preview.bmp");
    for (int i = 0; i < 20; ++i)
        REQUIRE(game->receiveServerMessage("History message " + std::to_string(i)));
    scene->render(canvas.app);
    const SDL_Rect history{30, 510, 396, 142};
    const auto latest = canvas.checksum(history);
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.x = 100;
    event.motion.y = 550;
    scene->handleEvents(canvas.app, event);
    event = {};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.y = 3;
    scene->handleEvents(canvas.app, event);
    scene->render(canvas.app);
    REQUIRE(canvas.checksum(history) != latest);
    // Reset the test history for a readable preview of the real map scene.
    game->chatSession().reset(game->chatSession().localSpeaker());
    REQUIRE(game->receiveServerMessage(u8"Witaj! Naciśnij Enter, aby napisać wiadomość."));
    REQUIRE(game->receiveChatMessage({ChatSource::Player, game->chatSession().localSpeaker(), "", u8"Cześć! Czy ktoś jest w pobliżu?"}));
    REQUIRE(game->receiveServerMessage(u8"Wiadomości mogą pochodzić od graczy, NPC i serwera."));
    scene->render(canvas.app);
    canvas.savePreview("chat-scene-preview.bmp");
    event = key(SDLK_RETURN);
    scene->handleEvents(canvas.app, event);
    event = text(u8"Piszę wiadomość...");
    scene->handleEvents(canvas.app, event);
    scene->render(canvas.app);
    canvas.savePreview("chat-editing-preview.bmp");
    scene->onExit(canvas.app);
}
}
