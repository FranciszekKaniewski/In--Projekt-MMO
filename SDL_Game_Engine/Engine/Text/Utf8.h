#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace utf8 {
inline std::size_t characterSize(std::string_view value, std::size_t offset) {
    const auto first = static_cast<unsigned char>(value[offset]);
    if (first < 0x80) return 1;
    const std::size_t size = first >= 0xC2 && first <= 0xDF ? 2 :
        first >= 0xE0 && first <= 0xEF ? 3 : first >= 0xF0 && first <= 0xF4 ? 4 : 0;
    if (!size || size > value.size() - offset) return 0;
    std::uint32_t code = first & (0x7F >> size);
    for (std::size_t i = 1; i < size; ++i) {
        const auto next = static_cast<unsigned char>(value[offset + i]);
        if ((next & 0xC0) != 0x80) return 0;
        code = (code << 6) | (next & 0x3F);
    }
    if ((size == 2 && code < 0x80) || (size == 3 && code < 0x800) ||
        (size == 4 && code < 0x10000) || code > 0x10FFFF ||
        (code >= 0xD800 && code <= 0xDFFF)) return 0;
    return size;
}

inline std::size_t count(std::string_view value) {
    std::size_t result = 0;
    for (std::size_t i = 0; i < value.size();) {
        const auto size = characterSize(value, i);
        i += size ? size : 1;
        if (size) ++result;
    }
    return result;
}

inline std::string singleLine(std::string_view value,
                              std::size_t limit = std::numeric_limits<std::size_t>::max()) {
    std::string result;
    std::size_t characters = 0;
    for (std::size_t i = 0; i < value.size() && characters < limit;) {
        const auto size = characterSize(value, i);
        if (!size) { ++i; continue; }
        const auto first = static_cast<unsigned char>(value[i]);
        if (first == '\n' || first == '\r' || first == '\t') {
            result += ' ';
            ++characters;
        } else if (first >= 0x20 && first != 0x7F) {
            result.append(value.data() + i, size);
            ++characters;
        }
        i += size;
    }
    return result;
}
}