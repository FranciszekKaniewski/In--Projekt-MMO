#pragma once

#include "../../Math/Vector2.h"

#include <map>
#include <string>

struct VelocityComponent {
    std::map<std::string, math::Vector2> vectors;

    math::Vector2 total() const noexcept {
        math::Vector2 result{};
        for (const auto& contribution : vectors) {
            result += contribution.second;
        }
        return result;
    }
};