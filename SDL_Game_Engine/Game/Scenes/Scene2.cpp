#include "../../Engine/Scenes/UIScene.h"

class UIScene2 : public UIScene {
public:
    UIScene2(SDL_Color color, const std::string& backgroundImage = "") : UIScene(color, backgroundImage) {}

    void onEnter(App& app) override {
        std::cout << "Entering Scene 2!" << std::endl;
    }
};

Scene* createScene2() {
    return new UIScene2({100, 100, 255, 255}, "assets/UI/MainMenuBG2.png");
}