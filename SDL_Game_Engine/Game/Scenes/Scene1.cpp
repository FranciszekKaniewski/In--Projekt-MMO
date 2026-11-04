#include "../../Engine/Scenes/UIScene.h"
#include "../../Engine/Scenes/SceneManager.h"
#include <utility>

class UIScene1 : public UIScene {
    Input* loginInput = nullptr;
    Input* passwordInput = nullptr;
    Label* status = nullptr;
    Button* play = nullptr;
    Button* settingsButton = nullptr;
    RequestId pending = 0;
    bool enteringCharacters = false;

    void setBusy(bool busy) {
        play->setEnabled(!busy);
        settingsButton->setEnabled(!busy);
        play->updateText(busy ? u8"Logowanie..." : "Play");
    }

    void logIn(App& app) {
        if(pending) return;
        if(loginInput->getText().empty() || passwordInput->getText().empty()) {
            status->updateText(u8"Wpisz login i hasło.");
            return;
        }
        app.audio.playSound("button.click");
        try {
            pending = app.gameClient().login(loginInput->getText(), passwordInput->getText());
            passwordInput->setText("");
            status->updateText(u8"Łączenie z serwerem...");
            setBusy(true);
        } catch(const std::exception&) {
            status->updateText(serverErrorText("connection_failed"));
        }
    }

public:
    UIScene1(SDL_Color color, const std::string& backgroundImage = "") : UIScene(color, backgroundImage) {}

    void onEnter(App& app) override {
        if(app.serverClient) app.serverClient->disconnect();
        uiLayer = new UILayer(app);
        uiLayer->addBox({-1,25,600,128}, {0,0,0,124}, 24);
        auto* panel = uiLayer->addBox({-1,-1,600,400}, {0,0,0,124}, 24, "", false, "assets/UI/WindowBG2.png");
        uiLayer->addBox({-1,app.getUISize().height-64-25,1000,64}, {0,0,0,124}, 24, "", false);

        Font titleFont("assets/fonts/Lato-Bold.ttf", 64, {255,255,255,255});
        Font buttonFont("assets/fonts/Lato-Bold.ttf", 32, {255,255,255,255});
        Font inputFont("assets/fonts/Lato-Regular.ttf", 24);
        Font messageFont("assets/fonts/Lato-Regular.ttf", 20, {130,35,30,255});
        uiLayer->getBoxByIndex(0)->addLabel(titleFont,"Super Game !",{0,0,0,0},{-1,-1,512,96});

        loginInput = panel->addInput(inputFont, u8"Login...", {-1,48,320,48});
        loginInput->setMaxLength(32);
        passwordInput = panel->addInput(inputFont, u8"Password...", {-1,112,320,48}, Input::Type::Password);
        passwordInput->setMaxLength(128);
        loginInput->onSubmit = [this, &app](const std::string&) { logIn(app); };
        passwordInput->onSubmit = [this, &app](const std::string&) { logIn(app); };

        play = panel->addButton(buttonFont, "Play", {0,0,0,0}, {88,192,200,64}, 1.05f,
            [this, &app]() { logIn(app); }, "assets/UI/btn.png");
        settingsButton = panel->addButton(buttonFont, "Settings", {0,0,0,0}, {312,192,200,64}, 1.05f,
            [&app]() { app.audio.playSound("button.click"); App::sceneManager->changeScene(createSettingsScene, app); }, "assets/UI/btn.png");
        status = panel->addLabel(messageFont, std::exchange(app.loginMessage, {}), {0,0,0,0}, {24,260,552,26});
        panel->addButton(buttonFont, "Exit", {0,0,0,0}, {-1,304,200,64}, 1.05f,
            [&app]() { app.audio.playSound("button.click"); app.isRunning = false; }, "assets/UI/btn.png");

        Font font1("assets/fonts/Lato-Regular.ttf", 20, {255,255,255,255});
        uiLayer->getBoxByIndex(2)->addLabel(font1,"Franciszek Kaniewski",{0,0,0,40},{52,-1,192,32});
        uiLayer->getBoxByIndex(2)->addLabel(font1,u8"Uniwersytet Mikołaja Kopernika w Toruniu",{0,0,0,40},{-1,-1,512,32});
        uiLayer->getBoxByIndex(2)->addLabel(font1,"2026/2027",{0,0,0,40},{814,-1,128,32});
    }

    void update(App& app) override {
        if(!app.serverClient) return;
        while(auto reply = app.serverClient->poll()) {
            if(!pending || reply->request != pending) continue;
            pending = 0;
            setBusy(false);
            if(reply->type == ServerReply::Type::Login && app.serverClient->isAuthenticated()) {
                enteringCharacters = true;
                App::sceneManager->changeScene(createCharactersScene, app);
                return;
            }
            status->updateText(serverErrorText(reply->error));
        }
    }

    void onExit(App& app) override {
        if(!enteringCharacters && app.serverClient) app.serverClient->disconnect();
        UIScene::onExit(app);
        loginInput = passwordInput = nullptr;
        status = nullptr; play = settingsButton = nullptr;
    }

    void handleEvents(App&, SDL_Event& event) override {
        handleUIEvents(event);
    }
};

Scene* createScene1() {
    return new UIScene1({255, 100, 100, 255}, "assets/UI/MainMenuBG2.png");
}
