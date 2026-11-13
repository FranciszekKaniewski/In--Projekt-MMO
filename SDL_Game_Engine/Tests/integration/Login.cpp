#include <catch_amalgamated.hpp>
#define SDL_MAIN_HANDLED
#include "../../Engine/Scenes/SceneManager.h"
#include "../../Engine/Scenes/UIScene.h"
#include "../../Engine/Scenes/GameScene.h"
#include "../support/GameSceneFixture.h"
#include <SDL_image.h>
#include <deque>
#include <cstring>
#include <fstream>
#include <iterator>

namespace {
class FakeGameClient : public GameClient {
public:
    bool authenticated = false;
    int disconnects = 0, logins = 0;
    int mapDownloads = 0;
    RequestId current = 0;
    std::string username, password;
    std::vector<std::int64_t> cursors, selections;
    std::deque<ServerReply> replies;
    std::optional<CharacterState> selected;
    RequestId login(std::string name, std::string secret) override {
        ++logins; username = std::move(name); password = std::move(secret); return ++current;
    }
    RequestId listCharacters(std::int64_t cursor) override { cursors.push_back(cursor); return ++current; }
    RequestId selectCharacter(std::int64_t id) override { selections.push_back(id); return ++current; }
    RequestId downloadMap() override { ++mapDownloads; return ++current; }
    std::optional<ServerReply> poll() override {
        if(replies.empty()) return std::nullopt;
        auto reply = std::move(replies.front()); replies.pop_front(); return reply;
    }
    bool isAuthenticated() const override { return authenticated; }
    std::optional<CharacterState> selectedCharacter() const override { return selected; }
    void disconnect() override { ++disconnects; authenticated = false; selected.reset(); replies.clear(); }
    void success(ServerReply::Type type) {
        ServerReply reply; reply.request = current; reply.type = type;
        if(type == ServerReply::Type::Login) authenticated = true;
        replies.push_back(std::move(reply));
    }
    void list(std::vector<CharacterSummary> rows, bool more = false) {
        ServerReply reply; reply.request = current; reply.type = ServerReply::Type::Characters;
        reply.characters = std::move(rows); reply.hasMore = more; replies.push_back(std::move(reply));
    }
    void error(const std::string& code, bool unsolicited = false) {
        ServerReply reply; reply.request = unsolicited ? 0 : current; reply.error = code;
        replies.push_back(std::move(reply));
    }
};

struct LoginCanvas {
    App app;
    SDL_Surface* surface = nullptr;
    FakeGameClient* client = nullptr;
    LoginCanvas() {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
        REQUIRE(TTF_Init() == 0);
        REQUIRE((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != 0);
        surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA32);
        REQUIRE(surface);
        App::renderer = SDL_CreateSoftwareRenderer(surface);
        REQUIRE(App::renderer);
        SDL_RenderSetLogicalSize(App::renderer, 1280, 720);
        App::sceneManager = new SceneManager();
        App::sceneManager->changeScene(createScene1, app);
        auto fake = std::make_unique<FakeGameClient>();
        client = fake.get(); app.serverClient = std::move(fake);
    }
    ~LoginCanvas() {
        app.clean();
        SDL_FreeSurface(surface);
        TTF_Quit(); IMG_Quit(); SDL_Quit();
    }
    UIScene* ui() {
        auto* scene = dynamic_cast<UIScene*>(App::sceneManager->activeScene);
        REQUIRE(scene); return scene;
    }
    UIBox* loginPanel() { return ui()->uiLayer->getBoxByIndex(1); }
    UIBox* characterPanel() { return ui()->uiLayer->getBoxByIndex(0); }
    void credentials() {
        auto* name = loginPanel()->getInputAtPoint(500, 220);
        auto* secret = loginPanel()->getInputAtPoint(500, 285);
        REQUIRE(name); REQUIRE(secret);
        name->setText("Franek"); secret->setText("Pass123!");
    }
    void click(int x, int y) {
        SDL_Event event{};
        event.type = SDL_MOUSEBUTTONDOWN; event.button.button = SDL_BUTTON_LEFT;
        event.button.x = x; event.button.y = y;
        App::sceneManager->handleEvents(app, event);
    }
    void update() { App::sceneManager->update(app); }
    void openCharacters() {
        App::sceneManager->closeScene(app);
        client->authenticated = true;
        App::sceneManager->changeScene(createCharactersScene, app);
    }
    void preview(const char* path) {
        App::sceneManager->render(app); SDL_RenderPresent(App::renderer);
        REQUIRE(IMG_SavePNG(surface, path) == 0);
    }
};

TEST_CASE("Play waits for server login and shows failure before allowing a retry", "[integration][login]") {
    LoginCanvas canvas;
    Scene* login = App::sceneManager->activeScene;
    canvas.credentials(); canvas.click(520, 380);
    REQUIRE(canvas.client->logins == 1);
    REQUIRE(canvas.client->username == "Franek");
    REQUIRE(canvas.client->password == "Pass123!");
    REQUIRE(canvas.loginPanel()->getInputAtPoint(500, 285)->getText().empty());
    canvas.click(520, 380); canvas.update();
    REQUIRE(canvas.client->logins == 1);
    REQUIRE(App::sceneManager->activeScene == login);
    canvas.client->error("invalid_credentials"); canvas.update();
    REQUIRE(App::sceneManager->activeScene == login);
    REQUIRE(canvas.loginPanel()->getElementAtPoint(520, 380));
    canvas.preview("Login-ui.png");
    canvas.credentials(); canvas.click(520, 380);
    canvas.client->success(ServerReply::Type::Login); canvas.update();
    REQUIRE(canvas.client->logins == 2);
    REQUIRE(canvas.client->isAuthenticated());
    REQUIRE(canvas.client->disconnects == 0);
    REQUIRE(canvas.client->cursors == std::vector<std::int64_t>{0});
    REQUIRE_FALSE(dynamic_cast<GameScene*>(App::sceneManager->activeScene));
}

TEST_CASE("Empty login and Space cannot bypass authentication", "[integration][login]") {
    LoginCanvas canvas;
    Scene* login = App::sceneManager->activeScene;
    SDL_Event event{}; event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_SPACE;
    App::sceneManager->handleEvents(canvas.app, event);
    canvas.click(520, 380); canvas.update();
    REQUIRE(canvas.client->logins == 0);
    REQUIRE(App::sceneManager->activeScene == login);
}

TEST_CASE("Character pages use server IDs and selection waits for server confirmation", "[integration][login][characters]") {
    LoginCanvas canvas;
    canvas.openCharacters();
    std::vector<CharacterSummary> first;
    for(int i = 1; i <= 10; ++i) first.push_back({i, "Hero " + std::to_string(i)});
    canvas.client->list(first, true); canvas.update();
    canvas.preview("Characters-list-ui.png");
    REQUIRE_FALSE(canvas.characterPanel()->getElementAtPoint(880, 604));
    canvas.click(720, 518);
    REQUIRE(canvas.client->cursors.back() == 10);
    canvas.client->list({{9007199254741009LL, u8"Żółw"}}); canvas.update();
    canvas.click(500, 204);
    canvas.preview("Characters-ui.png");
    canvas.click(880, 604);
    REQUIRE(canvas.client->selections == std::vector<std::int64_t>{9007199254741009LL});
    REQUIRE_FALSE(dynamic_cast<GameScene*>(App::sceneManager->activeScene));
    canvas.client->error("character_not_found"); canvas.update();
    REQUIRE_FALSE(dynamic_cast<GameScene*>(App::sceneManager->activeScene));
    canvas.click(880, 604);
    CharacterState state;
    state.id = 9007199254741009LL; state.name = u8"Żółw"; state.mapKey = "map_expanded";
    state.x = 1350; state.y = 1320; state.facing = 4;
    canvas.client->selected = state;
    ServerReply reply; reply.request = canvas.client->current;
    reply.type = ServerReply::Type::CharacterSelected; reply.character = state;
    canvas.client->replies.push_back(reply); canvas.update();
    REQUIRE(canvas.client->mapDownloads == 1);
    REQUIRE_FALSE(dynamic_cast<GameScene*>(App::sceneManager->activeScene));
    ServerReply downloaded;
    downloaded.request = canvas.client->current;
    downloaded.type = ServerReply::Type::MapData;
    downloaded.mapData = "{}";
    canvas.client->replies.push_back(downloaded); canvas.update();
    REQUIRE_FALSE(dynamic_cast<GameScene*>(App::sceneManager->activeScene));
    REQUIRE(canvas.client->isAuthenticated());
    canvas.click(880, 604);
    reply.request = canvas.client->current;
    canvas.client->replies.push_back(reply); canvas.update();
    REQUIRE(canvas.client->mapDownloads == 2);
    std::ifstream input(std::filesystem::path(serverMaps) / "Map_expanded.tmj", std::ios::binary);
    REQUIRE(input.good());
    downloaded.request = canvas.client->current;
    downloaded.mapData.assign(std::istreambuf_iterator<char>(input), {});
    canvas.client->replies.push_back(std::move(downloaded)); canvas.update();
    auto* game = dynamic_cast<GameScene*>(App::sceneManager->activeScene);
    REQUIRE(game);
    REQUIRE(canvas.client->isAuthenticated());
    SDL_Event enter{}; enter.type = SDL_KEYDOWN; enter.key.keysym.sym = SDLK_RETURN;
    App::sceneManager->handleEvents(canvas.app, enter);
    SDL_Event text{}; text.type = SDL_TEXTINPUT; std::strcpy(text.text.text, "Hello");
    App::sceneManager->handleEvents(canvas.app, text);
    App::sceneManager->handleEvents(canvas.app, enter);
    REQUIRE(game->chatSession().messages().back().author == u8"Żółw");
}

TEST_CASE("Empty character list can be refreshed and logout clears the connection", "[integration][characters]") {
    LoginCanvas canvas;
    canvas.openCharacters(); canvas.client->list({}); canvas.update();
    REQUIRE_FALSE(canvas.characterPanel()->getElementAtPoint(880, 604));
    canvas.click(880, 604); REQUIRE(canvas.client->selections.empty());
    canvas.click(900, 518); REQUIRE(canvas.client->cursors.size() == 2);
    REQUIRE(canvas.client->cursors.back() == 0);
    canvas.client->list({}); canvas.update();
    canvas.click(400, 604);
    REQUIRE_FALSE(canvas.client->isAuthenticated());
    REQUIRE(canvas.loginPanel()->getInputAtPoint(500, 220));
}

TEST_CASE("Expired session returns character selection to login", "[integration][characters]") {
    LoginCanvas canvas;
    canvas.openCharacters(); canvas.client->list({{12, "Knight"}}); canvas.update();
    canvas.client->authenticated = false;
    canvas.client->error("session_expired", true); canvas.update();
    REQUIRE(canvas.loginPanel()->getInputAtPoint(500, 220));
    REQUIRE_FALSE(canvas.client->isAuthenticated());
}
}
