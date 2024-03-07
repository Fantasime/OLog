#include <catch2/catch_test_macros.hpp>
#include <ctime>

#include "utils.h"

using namespace olog::utils;

TEST_CASE("Get current time", "[GetMsSystemClockInterval]") {
    time_t start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    int64_t cur_ms = GetMsSystemClockInterval();
    time_t cur_time = cur_ms / 1000;

    char start_time_str[std::size("YYYY-MM-DD hh:mm:ss.mil")] = {};
    char cur_time_str[std::size("YYYY-MM-DD hh:mm:ss.mil")] = {};
    strftime(start_time_str, sizeof(start_time_str), "%Y-%m-%d %H:%M:%S", localtime(&start_time));
    strftime(cur_time_str, sizeof(cur_time_str), "%Y-%m-%d %H:%M:%S", localtime(&cur_time));

    REQUIRE(strcmp(start_time_str, cur_time_str) == 0);
}