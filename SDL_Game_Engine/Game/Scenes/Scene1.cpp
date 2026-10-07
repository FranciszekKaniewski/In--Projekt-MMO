#include "../../Engine/Scenes/UIScene.h"
#include "../../Engine/Scenes/SceneManager.h"

class UIScene1 : public UIScene {
public:
    UIScene1(SDL_Color color) : UIScene(color) {}

    void onEnter(App& app) override {
        std::cout << "Entering Scene 1!" << std::endl;

        uiLayer = new UILayer(app);
        uiLayer->addBox({-1,25,600,128}, {200,200,200,255}, 24);
        uiLayer->addBox({-1,-1,600,400});
        uiLayer->addBox({-1,app.getUISize().height-64-25,1000,64}, {180,180,180,255}, 24);

        Font titleFont("assets/fonts/Lato-Bold.ttf", 64);
        Font buttonFont("assets/fonts/Lato-Bold.ttf", 32);
        Font inputFont("assets/fonts/Lato-Regular.ttf", 24);
        uiLayer->getBoxByIndex(0)->addLabel(titleFont,"Super Game !",{0,0,0,40},{-1,-1,512,96});

        Input* loginInput = uiLayer->getBoxByIndex(1)->addInput(
            inputFont, u8"Login...", {-1,24,320,48});
        loginInput->onSubmit = [](const std::string& name){
            std::cout << "Login: " << name << '\n';
        };
        Input* passwordInput = uiLayer->getBoxByIndex(1)->addInput(
                inputFont, u8"Password...", {-1,24*2+48,320,48});
        passwordInput->onSubmit = [](const std::string& name){
            std::cout << "Password: " << name << '\n';
        };

        Button* btn1 = new Button(buttonFont, "Start", {60,120,220,255}, {88,192,200,64}, 1.05f,
                                  [&app](){App::sceneManager->changeScene(createScene2, app);});
        Button* btn2 = new Button(buttonFont, "Settings", {220,120,60,255}, {312,192,200,64}, 1.05f,
                                  [&app](){App::sceneManager->changeScene(createSettingsScene, app);});
        Button* btn3 = new Button(buttonFont, "Exit", {120,60,220,255}, {-1,288,200,64}, 1.05f,
                                  [&app](){app.isRunning = false;});
        uiLayer->getBoxByIndex(1)->addButton(btn1);
        uiLayer->getBoxByIndex(1)->addButton(btn2);
        uiLayer->getBoxByIndex(1)->addButton(btn3);

        Font font1("assets/fonts/Lato-Regular.ttf", 20);
        uiLayer->getBoxByIndex(2)->addLabel(font1,"Franciszek Kaniewski",{0,0,0,40},{500-256-192 - (500-256-192)/2,-1,192,32});
        uiLayer->getBoxByIndex(2)->addLabel(font1,"Uniwersytet Mikołaja Kopernika w Toruniu",{0,0,0,40},{-1,-1,512,32});
        uiLayer->getBoxByIndex(2)->addLabel(font1,"2026/2027",{0,0,0,40},{500+256+128 + (500-256-128)/2 - 128,-1,128,32});
    }

    void onExit(App& app) override {
        delete uiLayer;
        uiLayer = nullptr;
    }

    void handleEvents(App& app, SDL_Event &event) override {
        if(handleUIEvents(event)) return;
        if (event.type == SDL_KEYDOWN) {

            if (event.key.keysym.sym == SDLK_SPACE) {
                app.sceneManager->changeScene(createScene2, app);
            }
        }
    }
};

Scene* createScene1() {
    return new UIScene1({255, 100, 100, 255});
}
