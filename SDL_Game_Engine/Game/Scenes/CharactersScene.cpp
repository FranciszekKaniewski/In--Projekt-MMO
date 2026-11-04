#include "../../Engine/Scenes/UIScene.h"
#include "../../Engine/Scenes/SceneManager.h"
#include <algorithm>

class CharactersScene : public UIScene {
    std::vector<CharacterSummary> characters;
    std::vector<Button*> rows;
    std::vector<std::int64_t> cursors{0};
    std::int64_t selectedId = 0;
    std::size_t page = 0, requestedPage = 0;
    std::int64_t requestedCursor = 0;
    RequestId pending = 0;
    bool selecting = false, hasMore = false, enteringGame = false, loaded = false;
    Label* status = nullptr;
    Button* play = nullptr;
    Button* previous = nullptr;
    Button* next = nullptr;
    Button* refresh = nullptr;
    std::string message;

    void setMessage(std::string text) {
        message = std::move(text);
        if(status) status->updateText(message);
    }

    void updateControls() {
        play->setEnabled(!pending && selectedId != 0);
        previous->setEnabled(!pending && page > 0);
        next->setEnabled(!pending && hasMore);
        refresh->setEnabled(!pending);
        for(auto* row : rows) row->setEnabled(!pending);
    }

    void loadPage(App& app, std::size_t target, std::int64_t cursor) {
        if(pending) return;
        requestedPage = target; requestedCursor = cursor; selecting = false;
        pending = app.gameClient().listCharacters(cursor);
        setMessage(u8"Pobieranie listy postaci...");
        updateControls();
    }

    void choose(std::int64_t id) {
        if(pending) return;
        selectedId = id;
        for(std::size_t i = 0; i < rows.size(); ++i) {
            rows[i]->updateText((characters[i].id == id ? "> " : "") + characters[i].name);
            rows[i]->bgColor = characters[i].id == id ? SDL_Color{50,84,130,255} : SDL_Color{45,50,62,225};
        }
        setMessage(u8"Postać wybrana. Kliknij Play, aby rozpocząć.");
        updateControls();
    }

    void enter(App& app) {
        if(pending || !selectedId) return;
        app.audio.playSound("button.click");
        selecting = true;
        pending = app.gameClient().selectCharacter(selectedId);
        setMessage(u8"Wybieranie postaci na serwerze...");
        updateControls();
    }

    void back(App& app) {
        if(app.serverClient) app.serverClient->disconnect();
        App::sceneManager->changeScene(createScene1, app);
    }

    void buildUI(App& app) {
        delete uiLayer;
        uiLayer = new UILayer(app);
        rows.clear();
        auto* panel = uiLayer->addBox({-1,-1,800,560}, {20,22,30,230}, 24, "", false, "assets/UI/WindowBG2.png");
        Font titleFont("assets/fonts/Lato-Bold.ttf", 40, {45,37,34,255});
        Font font("assets/fonts/Lato-Regular.ttf", 22, {255,255,255,255});
        Font inkFont("assets/fonts/Lato-Regular.ttf", 22, {65,53,49,255});
        Font buttonFont("assets/fonts/Lato-Bold.ttf", 24, {255,255,255,255});
        Font messageFont("assets/fonts/Lato-Regular.ttf", 18, {130,35,30,255});
        panel->addLabel(titleFont, u8"Wybierz postać", {0,0,0,0}, {40,16,720,52});
        panel->addLabel(inkFont, u8"Postacie przypisane do Twojego konta", {0,0,0,0}, {40,68,720,30});
        for(std::size_t i = 0; i < characters.size(); ++i) {
            const auto& character = characters[i];
            auto* row = panel->addButton(font, (character.id == selectedId ? "> " : "") + character.name,
                character.id == selectedId ? SDL_Color{50,84,130,255} : SDL_Color{45,50,62,225},
                {60,110 + static_cast<int>(i) * 30,680,28}, 1.01f,
                [this, id = character.id]() { choose(id); });
            row->margin = 6;
            row->bgCornerRadius = 8;
            rows.push_back(row);
        }
        if(characters.empty()) panel->addLabel(inkFont,
            !loaded ? u8"Oczekiwanie na listę postaci..." : u8"Na tym koncie nie ma jeszcze postaci.",
            {0,0,0,0}, {40,210,720,60});

        previous = panel->addButton(buttonFont, u8"Poprzednia", {0,0,0,0}, {60,420,170,36}, 1.03f,
            [this, &app]() { if(page) loadPage(app, page - 1, cursors[page - 1]); }, "assets/UI/btn.png");
        panel->addLabel(inkFont, u8"Strona " + std::to_string(page + 1), {0,0,0,0}, {245,420,140,36});
        next = panel->addButton(buttonFont, u8"Następna", {0,0,0,0}, {400,420,160,36}, 1.03f,
            [this, &app]() { if(hasMore && !characters.empty()) loadPage(app, page + 1, characters.back().id); }, "assets/UI/btn.png");
        refresh = panel->addButton(buttonFont, u8"Odśwież", {0,0,0,0}, {580,420,160,36}, 1.03f,
            [this, &app]() { loadPage(app, page, cursors[page]); }, "assets/UI/btn.png");
        status = panel->addLabel(messageFont, message, {0,0,0,0}, {30,464,740,28});
        panel->addButton(buttonFont, u8"Wyloguj", {0,0,0,0}, {60,502,200,44}, 1.05f,
            [this, &app]() { back(app); }, "assets/UI/btn.png");
        play = panel->addButton(buttonFont, "Play", {0,0,0,0}, {540,502,200,44}, 1.05f,
            [this, &app]() { enter(app); }, "assets/UI/btn.png");
        updateControls();
    }

public:
    CharactersScene() : UIScene({30,30,40,255}, "assets/UI/MainMenuBG2.png") {}

    void onEnter(App& app) override {
        buildUI(app);
        if(!app.serverClient || !app.serverClient->isAuthenticated()) {
            app.loginMessage = serverErrorText("authentication_required");
            App::sceneManager->changeScene(createScene1, app);
            return;
        }
        loadPage(app, 0, 0);
    }

    void update(App& app) override {
        if(!app.serverClient) return;
        while(auto reply = app.serverClient->poll()) {
            if(reply->request != pending && reply->request != 0) continue;
            if(reply->type == ServerReply::Type::Error) {
                pending = 0;
                if(!app.serverClient->isAuthenticated()) {
                    app.loginMessage = serverErrorText(reply->error);
                    App::sceneManager->changeScene(createScene1, app);
                    return;
                }
                setMessage(serverErrorText(reply->error));
                updateControls();
                continue;
            }
            if(!pending) continue;
            pending = 0;
            if(selecting && reply->type == ServerReply::Type::CharacterSelected && reply->character) {
                if(reply->character->mapKey != "map_expanded") {
                    setMessage(u8"Mapa tej postaci nie jest jeszcze dostępna.");
                    updateControls();
                    continue;
                }
                enteringGame = true;
                App::sceneManager->changeScene(createScene2, app);
                return;
            }
            if(!selecting && reply->type == ServerReply::Type::Characters) {
                page = requestedPage;
                cursors.resize(page + 1); cursors[page] = requestedCursor;
                characters = std::move(reply->characters);
                hasMore = reply->hasMore; loaded = true;
                if(std::none_of(characters.begin(), characters.end(), [this](const auto& c) { return c.id == selectedId; }))
                    selectedId = 0;
                message = characters.empty() ? u8"Lista jest pusta. Możesz ją odświeżyć." : u8"Kliknij postać na liście.";
                buildUI(app); // Replies are handled after UI event dispatch has finished.
            } else {
                setMessage(serverErrorText("invalid_response"));
                updateControls();
            }
        }
    }

    void handleEvents(App& app, SDL_Event& event) override {
        if(handleUIEvents(event)) return;
        if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && !event.key.repeat) back(app);
    }

    void onExit(App& app) override {
        if(!enteringGame && app.serverClient) app.serverClient->disconnect();
        UIScene::onExit(app);
        rows.clear(); status = nullptr; play = previous = next = refresh = nullptr;
    }
};

Scene* createCharactersScene() { return new CharactersScene(); }
