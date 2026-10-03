#include "../../Engine/Scenes/UIScene.h"
#include "../../Engine/Scenes/SceneManager.h"

class UIScene1 : public UIScene {
public:
    UIScene1(SDL_Color color) : UIScene(color) {}

    void onEnter(App& app) override {
        std::cout << "Entering Scene 1!" << std::endl;
    }

    void handleEvents(App& app, SDL_Event &event) override {
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