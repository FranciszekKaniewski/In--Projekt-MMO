#pragma once

#include "elements/Input.h"
#include "../Chat/Chat.h"

#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

struct CameraComponent;

class ChatOverlay {
public:
    ChatOverlay(ecs::Registry& world, ChatSession& session);
    bool handleEvent(const SDL_Event& event);
    bool isFocused() const { return input.isFocused(); }
    const std::string& draft() const { return input.getText(); }
    void render(const CameraComponent* camera, WindowSize screen);

private:
    static constexpr int textRasterScale = 2;
    struct TextImage {
        std::shared_ptr<SDL_Texture> texture;
        int width = 0;
        int height = 0;
    };
    struct BubbleImage {
        std::string text;
        TextImage image;
        bool seen = false;
    };

    ecs::Registry& world;
    ChatSession& session;
    Font font;
    Font smallFont;
    Input input;
    SDL_Rect panel{18, 474, 420, 228};
    SDL_Rect historyArea{};
    int mouseX = -1, mouseY = -1;
    int scrollOffset = 0;
    int historyHeight = 0;
    std::uint64_t cachedRevision = std::numeric_limits<std::uint64_t>::max();
    int cachedWidth = 0;
    std::vector<TextImage> messageImages;
    std::unordered_map<std::uint64_t, BubbleImage> bubbleImages;
    TextImage titleImage;
    TextImage statusImage;
    std::string cachedStatus;

    static TextImage makeText(const Font& font, const std::string& text, int wrapWidth, SDL_Color color);
    static void drawText(const TextImage& image, int x, int y, Uint8 alpha = 255, bool shadow = false);
    void layout(WindowSize screen);
    void rebuildHistory();
    void renderBubbles(const CameraComponent* camera, WindowSize screen);
};