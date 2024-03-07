#include "olog.h"

#include <catch2/catch_test_macros.hpp>
#include <thread>

TEST_CASE("OLOG won't change the variable", "[OLOG]") {
    OLOG(LogLevel::INFO, "Hello %*lf World!", 10, 3.1415);
    OLOG(LogLevel::INFO, "Hello %.*lf World!", 20, 3.1415);
    OLOG(LogLevel::INFO, "Hello %*.*lf World!", 10, 20, 3.1415);
    int a = 10;
    OLOG(LogLevel::INFO, "val: %d", a++);
    REQUIRE(a == 11);
    for (int i = 0; i < 3; ++i) {
        OLOG(LogLevel::INFO, "val: %d", ++a);
    }
    REQUIRE(a == 14);
}

TEST_CASE("OLOG in loop", "[OLOG]") {
    const char* str = "Everything is over.";

    for (int i = 0; i < std::size("Everything is over."); ++i) {
        OLOG(LogLevel::INFO, "%.*s %d", i, str, i);
    }
}