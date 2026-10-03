class Scene {
public:
    virtual ~Scene() = default;

    virtual void handleEvents(Game& game) = 0;
    virtual void update(Game& game) = 0;
    virtual void render(Game& game) = 0;
    virtual void onEnter(Game& game) {}
    virtual void onExit(Game& game) {}
};