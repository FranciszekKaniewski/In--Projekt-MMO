#pragma once

#include <cmath>

struct AnimationComponent {
    int row = 0;
    int frameCount = 1;
    int currentFrame = 0;
    double frameDuration = 0.2; // Seconds per frame.
    double elapsed = 0.0;
    bool loop = true;
    bool playing = true;

    void restart() noexcept {
        currentFrame = 0;
        elapsed = 0.0;
        playing = true;
    }

    bool isValid() const noexcept {
        return row >= 0 && frameCount > 0 && currentFrame >= 0 && currentFrame < frameCount &&
               frameDuration > 0.0 && std::isfinite(frameDuration) &&
               elapsed >= 0.0 && std::isfinite(elapsed);
    }
};
