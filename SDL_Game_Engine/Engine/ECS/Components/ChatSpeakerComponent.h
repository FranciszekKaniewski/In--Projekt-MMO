#pragma once

#include "../../Math/Vector2.h"
#include <string>

struct ChatSpeakerComponent {
    std::string name = "Gracz";
    bool npc = false;
    math::Vector2 bubbleOffset{0.0f, -64.0f};
};