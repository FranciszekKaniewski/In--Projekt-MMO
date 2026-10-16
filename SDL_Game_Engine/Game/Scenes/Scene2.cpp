#include "../../Engine/Scenes/GameScene.h"

class Scene2 : public GameScene {
public:
    Scene2() : GameScene() {}

    void onEnter(App& app) override {
        std::cout << "Entering Scene 2!" << std::endl;
    }
};

Scene* createScene2() {
    return new Scene2();
}