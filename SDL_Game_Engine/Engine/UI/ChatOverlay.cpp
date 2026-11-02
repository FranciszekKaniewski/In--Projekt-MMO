#include "ChatOverlay.h"

#include "../ECS/Components/CameraComponent.h"
#include "../ECS/Components/ChatSpeakerComponent.h"
#include "../ECS/Components/SpeechBubbleComponent.h"
#include "../ECS/Components/TransformComponent.h"

#include <algorithm>
#include <cmath>

namespace {
Font smoothFont(const char* path, int size, SDL_Color color) {
    Font font(path, size, color);
    if (font.font) TTF_SetFontHinting(font.font, TTF_HINTING_LIGHT);
    return font;
}

bool contains(SDL_Rect bounds, int x, int y) {
    const SDL_Point point{x, y};
    return SDL_PointInRect(&point, &bounds) == SDL_TRUE;
}

class ClipGuard {
    SDL_Rect previous{};
    bool enabled;
public:
    explicit ClipGuard(SDL_Rect area) : enabled(SDL_RenderIsClipEnabled(App::renderer) == SDL_TRUE) {
        SDL_RenderGetClipRect(App::renderer, &previous);
        if (enabled) {
            SDL_Rect intersection{};
            SDL_IntersectRect(&previous, &area, &intersection);
            area = intersection;
        }
        SDL_RenderSetClipRect(App::renderer, &area);
    }
    ~ClipGuard() { SDL_RenderSetClipRect(App::renderer, enabled ? &previous : nullptr); }
};
}

ChatOverlay::ChatOverlay(ecs::Registry& world, ChatSession& session)
    : world(world), session(session),
      font(smoothFont("Assets/fonts/Lato-Regular.ttf", 18 * textRasterScale, {229, 236, 243, 255})),
      smallFont(smoothFont("Assets/fonts/Lato-Regular.ttf", 14 * textRasterScale, {157, 179, 196, 255})),
      input(font, u8"Write message...", {28, 660, 400, 32}) {
    input.setMaxLength(ChatSession::maxCharacters);
    input.textRasterScale = textRasterScale;
    input.bgColor = {14, 23, 32, 70};
    input.borderColor = {49, 68, 84, 75};
    input.focusColor = {72, 181, 176, 95};
    input.placeholderColor = {122, 144, 161, 255};
    input.cornerRadius = 6;
    input.onSubmit = [this](const std::string& value) {
        this->session.send(this->world, value);
        input.setText("");
        input.setFocused(false);
        scrollOffset = 0;
    };
    titleImage = makeText(smallFont, "Chat", 0, {222, 235, 244, 255});
    layout(App::logicalSize);
}

ChatOverlay::TextImage ChatOverlay::makeText(const Font& font, const std::string& text,
                                           int wrapWidth, SDL_Color color) {
    TextImage result;
    if (!App::renderer || !font.font || text.empty()) return result;
    SDL_Surface* surface = wrapWidth > 0
        ? TTF_RenderUTF8_Blended_Wrapped(font.font, text.c_str(), color,
                                        static_cast<Uint32>(wrapWidth * textRasterScale))
        : TTF_RenderUTF8_Blended(font.font, text.c_str(), color);
    if (!surface) return result;
    result.width = (surface->w + textRasterScale - 1) / textRasterScale;
    result.height = (surface->h + textRasterScale - 1) / textRasterScale;
    result.texture = {SDL_CreateTextureFromSurface(App::renderer, surface), SDL_DestroyTexture};
    if (result.texture) SDL_SetTextureScaleMode(result.texture.get(), SDL_ScaleModeLinear);
    SDL_FreeSurface(surface);
    return result;
}

void ChatOverlay::drawText(const TextImage& image, int x, int y, Uint8 alpha, bool shadow) {
    if (!image.texture) return;
    if (shadow) {
        SDL_SetTextureColorMod(image.texture.get(), 0, 0, 0);
        SDL_SetTextureAlphaMod(image.texture.get(), static_cast<Uint8>(alpha * 0.75f));
        const SDL_Rect shadowRect{x + 1, y + 1, image.width, image.height};
        SDL_RenderCopy(App::renderer, image.texture.get(), nullptr, &shadowRect);
        SDL_SetTextureColorMod(image.texture.get(), 255, 255, 255);
    }
    SDL_SetTextureAlphaMod(image.texture.get(), alpha);
    const SDL_Rect destination{x, y, image.width, image.height};
    SDL_RenderCopy(App::renderer, image.texture.get(), nullptr, &destination);
}

void ChatOverlay::layout(WindowSize screen) {
    panel.w = std::min(420, std::max(100, screen.wight - 36));
    panel.h = std::min(228, std::max(90, screen.height - 36));
    panel.x = 18;
    panel.y = screen.height - panel.h - 18;
    historyArea = input.isFocused()
        ? SDL_Rect{panel.x + 12, panel.y + 36, panel.w - 24, panel.h - 86}
        : SDL_Rect{panel.x + 12, panel.y + 12, panel.w - 24, panel.h - 24};
    scrollOffset = std::clamp(scrollOffset, 0, std::max(0, historyHeight - historyArea.h));
    input.changeRect(panel.x + 10, panel.y + panel.h - 42, panel.w - 20, 32);
}

bool ChatOverlay::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION) {
        mouseX = event.motion.x;
        mouseY = event.motion.y;
    }
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        input.setFocused(false);
        return false;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        mouseX = event.button.x;
        mouseY = event.button.y;
        input.setFocused(input.containsPoint(mouseX, mouseY));
        return contains(panel, mouseX, mouseY);
    }
    if (event.type == SDL_MOUSEWHEEL && contains(panel, mouseX, mouseY)) {
        const int direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        scrollOffset = std::clamp(scrollOffset + event.wheel.y * direction * 48,
                                  0, std::max(0, historyHeight - historyArea.h));
        return true;
    }
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && input.isFocused()) {
        input.setText("");
        input.setFocused(false);
        return true;
    }
    if (event.type == SDL_KEYDOWN &&
        (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) &&
        !input.isFocused()) {
        if (!event.key.repeat) input.setFocused(true);
        return true;
    }
    return input.handleEvent(event);
}

void ChatOverlay::rebuildHistory() {
    if (cachedRevision == session.revision() && cachedWidth == historyArea.w) return;
    const int previousHeight = historyHeight;
    historyHeight = 0;
    messageImages.clear();
    for (const auto& message : session.messages()) {
        const SDL_Color color = message.source == ChatSource::Server ? SDL_Color{240, 202, 132, 255} :
            message.source == ChatSource::NPC ? SDL_Color{165, 218, 173, 255} : SDL_Color{224, 235, 244, 255};
        auto image = makeText(font, message.author + ": " + message.text, historyArea.w, color);
        historyHeight += image.height + 6;
        messageImages.push_back(std::move(image));
    }
    if (scrollOffset > 0) scrollOffset += historyHeight - previousHeight;
    scrollOffset = std::clamp(scrollOffset, 0, std::max(0, historyHeight - historyArea.h));
    cachedRevision = session.revision();
    cachedWidth = historyArea.w;
}

void ChatOverlay::renderBubbles(const CameraComponent* camera, WindowSize screen) {
    for (auto& entry : bubbleImages) entry.second.seen = false;
    world.forEach<TransformComponent, ChatSpeakerComponent, SpeechBubbleComponent>(
        [&](ecs::Entity entity, const TransformComponent& transform,
            const ChatSpeakerComponent& speaker, const SpeechBubbleComponent& bubble) {
            if (bubble.remaining <= 0.0f || !std::isfinite(bubble.remaining) ||
                !session.canShowBubble(world, entity)) return;
            auto anchor = transform.position + speaker.bubbleOffset;
            if (camera) {
                if (!camera->isValid()) return;
                anchor -= camera->position;
            }
            if (!std::isfinite(anchor.x) || !std::isfinite(anchor.y) ||
                anchor.x < 0 || anchor.y < 0 || anchor.x >= screen.wight || anchor.y >= screen.height) return;

            auto& cached = bubbleImages[entity.id()];
            cached.seen = true;
            if (cached.text != bubble.text || !cached.image.texture) {
                cached.text = bubble.text;
                const int maximumWidth = std::max(1, std::min(480, screen.wight - 40));
                int wrapWidth = std::min(256, maximumWidth);
                do {
                    cached.image = makeText(font, bubble.text, wrapWidth, {28, 38, 48, 255});
                    if (cached.image.height + 18 <= anchor.y - 28 || wrapWidth == maximumWidth) break;
                    wrapWidth = std::min(wrapWidth + 64, maximumWidth);
                } while (cached.image.texture);
            }
            if (!cached.image.texture) return;
            const int width = cached.image.width + 24;
            const int height = cached.image.height + 18;
            const int x = std::clamp(static_cast<int>(anchor.x) - width / 2,
                                     8, std::max(8, screen.wight - width - 8));
            const int y = std::max(8, static_cast<int>(anchor.y) - height - 10);
            const Uint8 alpha = static_cast<Uint8>(255.0f * std::min(1.0f, bubble.remaining));
            TextureManager::DrawRectangle({x + 2, y + 3, width, height}, {0, 0, 0, static_cast<Uint8>(alpha / 4)}, 9);
            TextureManager::DrawRectangle({x, y, width, height}, {64, 80, 94, alpha}, 9);
            TextureManager::DrawRectangle({x + 1, y + 1, width - 2, height - 2}, {248, 250, 252, alpha}, 8);
            const int tailX = std::clamp(static_cast<int>(anchor.x), x + 10, x + width - 10);
            for (int row = 0; row < 9; ++row)
                TextureManager::DrawRectangle({tailX - 8 + row, y + height - 1 + row, 17 - row * 2, 1},
                                               {248, 250, 252, alpha});
            drawText(cached.image, x + 12, y + 9, alpha);
        });
    for (auto it = bubbleImages.begin(); it != bubbleImages.end();) {
        if (!it->second.seen) it = bubbleImages.erase(it);
        else ++it;
    }
}

void ChatOverlay::render(const CameraComponent* camera, WindowSize screen) {
    if (!App::renderer || screen.wight < 80 || screen.height < 100) return;
    SDL_BlendMode previousBlend;
    SDL_GetRenderDrawBlendMode(App::renderer, &previousBlend);
    SDL_SetRenderDrawBlendMode(App::renderer, SDL_BLENDMODE_BLEND);
    layout(screen);
    rebuildHistory();
    renderBubbles(camera, screen);

    if (input.isFocused()) {
        TextureManager::DrawRectangle(panel, {12, 20, 28, 128}, 10);
        drawText(titleImage, panel.x + 14, panel.y + 12);
        const std::string status = std::to_string(input.getLength()) + "/256";
        if (status != cachedStatus) {
            statusImage = makeText(smallFont, status, 0,
                                  input.getLength() == ChatSession::maxCharacters
                                    ? SDL_Color{240, 202, 132, 255} : SDL_Color{180, 200, 216, 255});
            cachedStatus = status;
        }
        drawText(statusImage, panel.x + panel.w - statusImage.width - 14, panel.y + 12);
    }
    {
        ClipGuard clip(historyArea);
        int y = historyArea.y + historyArea.h - historyHeight + scrollOffset;
        for (const auto& image : messageImages) {
            if (y + image.height > historyArea.y && y < historyArea.y + historyArea.h)
                drawText(image, historyArea.x, y, 255, true);
            y += image.height + 6;
        }
    }
    if (input.isFocused()) input.draw();
    SDL_SetRenderDrawBlendMode(App::renderer, previousBlend);
}