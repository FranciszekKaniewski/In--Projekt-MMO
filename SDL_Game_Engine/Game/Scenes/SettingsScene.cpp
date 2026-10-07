#include "../../Engine/Scenes/UIScene.h"
#include "../../Engine/Scenes/SceneManager.h"

class SettingsScene : public UIScene {
private:
    int layoutWidth = 0;
    int layoutHeight = 0;

public:
    SettingsScene() : UIScene({255,100,100,255}) {}

    void onEnter(App& app) override {
        app.syncDisplaySettings();
        const WindowSize size = app.getUISize();
        layoutWidth = size.wight;
        layoutHeight = size.height;
        uiLayer = new UILayer(app);
        UIBox* panel = uiLayer->addBox({-1,-1,760,640}, {255,255,255,255}, 24);
        Font titleFont("assets/fonts/Lato-Bold.ttf", 40);
        Font bodyFont("assets/fonts/Lato-Regular.ttf", 24);
        Font hintFont("assets/fonts/Lato-Regular.ttf", 18, {110,116,128,255});
        Font buttonFont("assets/fonts/Lato-Bold.ttf", 28);

        panel->addLabel(titleFont, "Settings", {0,0,0,0}, {-1,32,480,56});
        panel->addLabel(hintFont, u8"Zmiany są stosowane od razu.",
                        {0,0,0,0}, {-1,94,600,28});

        auto addCaption = [&](const std::string& text, int y) {
            Label* label = panel->addLabel(bodyFont, text, {0,0,0,0}, {48,y,208,40});
            label->alignment = Label::Alignment::Left;
        };
        addCaption(u8"Głośność", 152);
        addCaption(u8"Wycisz dźwięk", 230);
        addCaption(u8"Pełny ekran", 308);
        addCaption(u8"Rozdzielczość", 382);
        addCaption("Limit FPS", 456);

        Label* volumeLabel = panel->addLabel(bodyFont,
            std::to_string(app.settings.volume) + "%", {0,0,0,0}, {628,152,84,40});
        panel->addSlider(0, 100, app.settings.volume, {280,152,330,40}, 1,
            [&app, volumeLabel](int value) {
                app.settings.volume = value;
                app.applyAudioSettings();
                volumeLabel->updateText(std::to_string(value) + "%");
            });

        panel->addCheckbox(app.settings.muted, {280,234,32,32},
            [&app](bool checked) {
                app.settings.muted = checked;
                app.applyAudioSettings();
            });

        panel->addCheckbox(app.settings.fullscreen, {280,312,32,32},
            [&app](bool checked) {
                app.setFullscreen(checked);
                App::sceneManager->changeScene(createSettingsScene, app);
            });

        const std::vector<WindowResolution> resolutions = app.getAvailableResolutions();
        std::vector<std::string> resolutionOptions;
        int selectedResolution = 0;
        for(std::size_t i = 0; i < resolutions.size(); ++i) {
            const auto& resolution = resolutions[i];
            resolutionOptions.push_back(std::to_string(resolution.width) + " x " +
                                         std::to_string(resolution.height));
            if(resolution.width == app.settings.width && resolution.height == app.settings.height)
                selectedResolution = static_cast<int>(i);
        }
        panel->addSelect(bodyFont, std::move(resolutionOptions), selectedResolution, {280,378,328,48},
            [&app, resolutions](int index, const std::string&) {
                const auto& resolution = resolutions[index];
                app.setResolution(resolution.width, resolution.height);
                App::sceneManager->changeScene(createSettingsScene, app);
            });

        std::vector<int> fpsLimits = {30,60,120,144,240};
        if(std::find(fpsLimits.begin(), fpsLimits.end(), app.settings.fpsLimit) == fpsLimits.end()) {
            fpsLimits.push_back(app.settings.fpsLimit);
            std::sort(fpsLimits.begin(), fpsLimits.end());
        }
        std::vector<std::string> fpsOptions;
        for(int fps : fpsLimits) fpsOptions.push_back(std::to_string(fps) + " FPS");
        const int selectedFPS = static_cast<int>(
            std::find(fpsLimits.begin(), fpsLimits.end(), app.settings.fpsLimit) - fpsLimits.begin());
        panel->addSelect(bodyFont, std::move(fpsOptions), selectedFPS, {280,452,328,48},
            [&app, fpsLimits](int index, const std::string&) {
                app.settings.fpsLimit = fpsLimits[index];
            });

        panel->addLabel(hintFont, u8"Tab: zmiana pola   ·   Strzałki: zmiana wartości",
                        {0,0,0,0}, {-1,524,680,24});
        panel->addButton(buttonFont, u8"Powrót", {60,120,220,255}, {-1,560,200,56}, 1.05f,
            [&app]() { App::sceneManager->changeScene(createScene1, app); });
    }

    void handleEvents(App& app, SDL_Event& event) override {
        if(event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(app.window) &&
           (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            event.window.event == SDL_WINDOWEVENT_RESIZED)) {
            app.syncDisplaySettings();
            const WindowSize size = app.getUISize();
            if(size.wight != layoutWidth || size.height != layoutHeight)
                App::sceneManager->changeScene(createSettingsScene, app);
            return;
        }
        if(handleUIEvents(event)) return;
        if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && !event.key.repeat)
            App::sceneManager->changeScene(createScene1, app);
    }

    void onExit(App& app) override {
        delete uiLayer;
        uiLayer = nullptr;
    }
};

Scene* createSettingsScene() {
    return new SettingsScene();
}
