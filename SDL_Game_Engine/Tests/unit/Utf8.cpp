#include <catch_amalgamated.hpp>
#include "../../Engine/Text/Utf8.h"

TEST_CASE("UTF-8 character limits preserve Polish letters and complete emoji", "[unit][chat][utf8]") {
    std::string value;
    for (int i = 0; i < 255; ++i) value += u8"ą";
    value += u8"😀ż";
    const auto limited = utf8::singleLine(value, 256);
    REQUIRE(utf8::count(limited) == 256);
    REQUIRE(limited.size() == 255 * 2 + 4);
    REQUIRE(limited.substr(limited.size() - 4) == u8"😀");
    REQUIRE(utf8::singleLine(value, 0).empty());
    REQUIRE(utf8::singleLine("hello\nworld\t!") == "hello world !");
    const std::string malformed = std::string("a") + "\xC0\xAF" + "\xED\xA0\x80" + "\xF4\x90\x80\x80" + "z\xC4";
    REQUIRE(utf8::singleLine(malformed) == "az");
}
