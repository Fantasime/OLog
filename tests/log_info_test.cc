#include "log_info.h"

#include <catch2/catch_test_macros.hpp>
#include <iostream>

using namespace olog::log_info;

TEST_CASE("FormatParametersCount", "[FormatParametersCount]") {
    const char str[] = "Hello World";
    // Ensure FormatParametersCount takes the constant expression branch
    constexpr size_t num_parameters = noexcept(FormatParametersCount(str));
    REQUIRE(num_parameters == 0);

    REQUIRE(FormatParametersCount("A string with no parameter.") == 0);
    REQUIRE(FormatParametersCount("count: %d\n") == 1);
    REQUIRE(FormatParametersCount(
                "Output a string with dynamic length: %20.*s") == 2);
}

TEST_CASE("GetParamInfo", "[GetParamInfo]") {
    REQUIRE(GetParamInfo("Hello World") == ParamType::INVALID);
    REQUIRE(GetParamInfo("%d") == ParamType::NON_STRING);
    REQUIRE(GetParamInfo("%*lf") == ParamType::DYNAMIC_WIDTH);
    REQUIRE(GetParamInfo("%*lf", 1) == ParamType::NON_STRING);
    REQUIRE(GetParamInfo("%.*lu") == ParamType::DYNAMIC_PRECISION);
    REQUIRE(GetParamInfo("%.*lu", 1) == ParamType::NON_STRING);
    REQUIRE(GetParamInfo("%*.*lu", 2) == ParamType::NON_STRING);
    REQUIRE(GetParamInfo("%s") == ParamType::STRING_WITH_NO_PRECISION);
    REQUIRE(GetParamInfo("%.*s", 1) ==
            ParamType::STRING_WITH_DYNAMIC_PRECISION);
    REQUIRE(GetParamInfo("%.23s") == ParamType(23));
    REQUIRE_THROWS(GetParamInfo("%"));
    REQUIRE_THROWS(GetParamInfo("%n"));

    REQUIRE(GetParamInfo("pad%17.31lcing") == ParamType::NON_STRING);
}

TEST_CASE("AnalyzeFormatParameters", "[AnalyzeFormatParameters]") {
    constexpr char fmt0[] = "Hello Wolrd";
    constexpr size_t num_params0 = FormatParametersCount(fmt0);
    constexpr auto params0 = AnalyzeFormatParameters<num_params0>(fmt0);
    REQUIRE(params0.size() == num_params0);

    constexpr char fmt1[] = "Hel%dlo Wo%lflrd";
    constexpr size_t num_params1 = FormatParametersCount(fmt1);
    constexpr auto params1 = AnalyzeFormatParameters<num_params1>(fmt1);
    REQUIRE(params1.size() == num_params1);
    REQUIRE(params1[0] == ParamType::NON_STRING);
    REQUIRE(params1[1] == ParamType::NON_STRING);

    constexpr char fmt2[] = "He%*.*dllo Wor%*.*sld";
    constexpr size_t num_params2 = FormatParametersCount(fmt2);
    constexpr auto params2 = AnalyzeFormatParameters<num_params2>(fmt2);
    REQUIRE(params2.size() == num_params2);
    REQUIRE(params2[0] == ParamType::DYNAMIC_WIDTH);
    REQUIRE(params2[1] == ParamType::DYNAMIC_PRECISION);
    REQUIRE(params2[2] == ParamType::NON_STRING);
    REQUIRE(params2[3] == ParamType::DYNAMIC_WIDTH);
    REQUIRE(params2[4] == ParamType::DYNAMIC_PRECISION);
    REQUIRE(params2[5] == ParamType::STRING_WITH_DYNAMIC_PRECISION);
}

TEST_CASE("GetConversionType", "[GetConversionType]") {
    // Signed integers.
    REQUIRE(GetConversionType("pad%17.31ding") == ConversionType::int_t);
    REQUIRE(GetConversionType("pad%17.31iing") == ConversionType::int_t);
    REQUIRE(GetConversionType("pad%17.31hhding") ==
            ConversionType::signed_char_t);
    REQUIRE(GetConversionType("pad%17.31hhiing") ==
            ConversionType::signed_char_t);
    REQUIRE(GetConversionType("pad%17.31llding") ==
            ConversionType::long_long_int_t);
    REQUIRE(GetConversionType("pad%17.31hding") == ConversionType::short_int_t);
    REQUIRE(GetConversionType("pad%17.31hiing") == ConversionType::short_int_t);
    REQUIRE(GetConversionType("pad%17.31ldng") == ConversionType::long_int_t);
    REQUIRE(GetConversionType("pad%17.31liing") == ConversionType::long_int_t);
    REQUIRE(GetConversionType("pad%17.31jding") == ConversionType::intmax_t_t);
    REQUIRE(GetConversionType("pad%17.31jiing") == ConversionType::intmax_t_t);
    REQUIRE(GetConversionType("pad%17.31zdng") == ConversionType::size_t_t);
    REQUIRE(GetConversionType("pad%17.31zing") == ConversionType::size_t_t);
    REQUIRE(GetConversionType("pad%17.31tdng") == ConversionType::ptrdiff_t_t);
    REQUIRE(GetConversionType("pad%17.31ting") == ConversionType::ptrdiff_t_t);

    // Unsigned integers.
    REQUIRE(GetConversionType("pad%17.31uing") ==
            ConversionType::unsigned_int_t);
    REQUIRE(GetConversionType("pad%17.31hhuing") ==
            ConversionType::unsigned_char_t);
    REQUIRE(GetConversionType("pad%17.31lluing") ==
            ConversionType::unsigned_long_long_int_t);
    REQUIRE(GetConversionType("pad%17.31huing") ==
            ConversionType::unsigned_short_int_t);
    REQUIRE(GetConversionType("pad%17.31luing") ==
            ConversionType::unsigned_long_int_t);
    REQUIRE(GetConversionType("pad%17.31juing") == ConversionType::uintmax_t_t);
    REQUIRE(GetConversionType("pad%17.31zuing") == ConversionType::size_t_t);
    REQUIRE(GetConversionType("pad%17.31tung") == ConversionType::ptrdiff_t_t);

    // Strings.
    REQUIRE(GetConversionType("pad%17.31sing") ==
            ConversionType::const_char_ptr_t);
    REQUIRE(GetConversionType("pad%17.31lsing") ==
            ConversionType::const_wchar_t_ptr_t);

    // Pointer.
    REQUIRE(GetConversionType("pad%17.31ping") ==
            ConversionType::const_void_ptr_t);

    // Floating points.
    REQUIRE(GetConversionType("pad%17.31fing") == ConversionType::double_t);
    REQUIRE(GetConversionType("pad%17.31lfing") == ConversionType::double_t);
    REQUIRE(GetConversionType("pad%17.31Lfng") ==
            ConversionType::long_double_t);

    // Characters.
    REQUIRE(GetConversionType("pad%17.31cing") == ConversionType::int_t);
    REQUIRE(GetConversionType("pad%17.31lcing") == ConversionType::wint_t_t);

    // NONE.
    REQUIRE(GetConversionType("A string without conversion specifier.") ==
            ConversionType::NONE);

    // Muti
    REQUIRE(GetConversionType(
                "pad%17.31Lfng, pad%17.31lcing,pad%17.31lluing%*.*lu", 1) ==
            ConversionType::wint_t_t);
    REQUIRE(GetConversionType(
                "pad%17.31Lfng, pad%17.31lcing,pad%17.31lluing%*.*lu", 2) ==
            ConversionType::unsigned_long_long_int_t);
    REQUIRE(GetConversionType(
                "pad%17.31Lfng, pad%17.31lcing,pad%17.31lluing%*.*lu", 3) ==
            ConversionType::unsigned_long_int_t);
}

TEST_CASE("ConversionSpecifiersCount", "[ConversionSpecifiersCount]") {
    REQUIRE(ConversionSpecifiersCount(
                "Current time is: %4u-%2u-%2u %2u:%2u:%2u") == 6);
    REQUIRE(ConversionSpecifiersCount("pad%17.31lcing") == 1);
}

TEST_CASE("SizeConversionStorageNeeds", "[SizeConversionStorageNeeds]") {
    REQUIRE(SizeConversionStorageNeeds("Hello World") == 0);
    REQUIRE(SizeConversionStorageNeeds("pad%17.31lcing") ==
            std::size("%17.31lc"));
    REQUIRE(SizeConversionStorageNeeds(
                "pad%17.31Lfng, pad%17.31lcing,pad%17.31lluing%*.*lu") ==
            std::size("%17.31Lf %17.31lc %17.31llu %*.*lu"));
}

TEST_CASE("MakeConversionStorage", "[MakeConversionStorage]") {
    constexpr char format[] =
        "pad%17.31Lfng, pad%17.31lcing,pad%17.31lluing%*.*lu";
    constexpr auto storage_size = SizeConversionStorageNeeds(format);
    constexpr auto storage = MakeConversionStorage<storage_size>(format);
    auto require =
        std::array<char, storage_size>{"%17.31Lf\0%17.31lc\0%17.31llu\0%*.*lu"};
    REQUIRE(storage == require);
}

TEST_CASE("GetConversionSpecifierPosition",
          "[GetConversionSpecifierPosition]") {
    constexpr char format[] =
        "pad%17.31Lfng, pad%17.31lcing,pad%17.31lluing%*.*lu";
    constexpr size_t pos0 = GetConversionSpecifierPosition(format, 0);
    REQUIRE(pos0 == 3);
    constexpr size_t pos1 = GetConversionSpecifierPosition(format, 1);
    REQUIRE(pos1 == 18);
    constexpr size_t pos2 = GetConversionSpecifierPosition(format, 2);
    REQUIRE(pos2 == 33);
    constexpr size_t pos3 = GetConversionSpecifierPosition(format, 3);
    REQUIRE(pos3 == 45);
}

TEST_CASE("GetConversionSpecifierPositionInStorage",
          "[GetConversionSpecifierPositionInStorage]") {
    constexpr char format[] =
        "pad%17.31Lfng, pad%17.31lcing,pad%17.31lluing%*.*lu";
    constexpr auto storage_size = SizeConversionStorageNeeds(format);
    constexpr auto storage = MakeConversionStorage<storage_size>(format);

    constexpr size_t pos0 = GetConversionSpecifierPositionInStorage(storage, 0);
    REQUIRE(pos0 == 0);

    constexpr size_t pos1 = GetConversionSpecifierPositionInStorage(storage, 1);
    REQUIRE(pos1 == std::size("%17.31Lf"));

    constexpr size_t pos2 = GetConversionSpecifierPositionInStorage(storage, 2);
    REQUIRE(pos2 == std::size("%17.31Lf\0%17.31lc"));

    constexpr size_t pos3 = GetConversionSpecifierPositionInStorage(storage, 3);
    REQUIRE(pos3 == std::size("%17.31Lf\0%17.31lc\0%17.31llu"));
}

TEST_CASE("GetFormatFragments", "[GetFormatFragments]") {
    constexpr char format[] =
        "pad%17.31Lfng, pad%17.31lcing,pad%17.31lluing%*.*lu";
    constexpr size_t num_fragments = ConversionSpecifiersCount(format);
    constexpr auto storage_size = SizeConversionStorageNeeds(format);
    constexpr auto storage = MakeConversionStorage<storage_size>(format);

    constexpr std::array<FormatFragment, num_fragments> format_fragments =
        GetFormatFragments<num_fragments, storage_size>(format, storage);
    constexpr std::array<FormatFragment, num_fragments> require{
        FormatFragment{GetConversionType("%17.31Lf"), std::size("%17.31Lf") - 1,
                       std::size("pad") - 1, 0},
        FormatFragment{GetConversionType("%17.31lc"), std::size("%17.31lc") - 1,
                       std::size("pad%17.31Lfng, pad") - 1,
                       std::size("%17.31Lf")},
        FormatFragment{GetConversionType("%17.31llu"),
                       std::size("%17.31llu") - 1,
                       std::size("pad%17.31Lfng, pad%17.31lcing,pad") - 1,
                       std::size("%17.31Lf\0%17.31lc")},
        FormatFragment{
            {GetConversionType("%*.*lu")},
            std::size("%*.*lu") - 1,
            std::size("pad%17.31Lfng, pad%17.31lcing,pad%17.31lluing") - 1,
            std::size("%17.31Lf\0%17.31lc\0%17.31llu")}};

    REQUIRE(format_fragments == require);
}

TEST_CASE("GetParamSizes", "[GetParamSizes]") {
    constexpr char format[] = "|%d|%f|%lf|%s|%x|%u|";
    constexpr size_t num_params = FormatParametersCount(format);
    auto param_types = AnalyzeFormatParameters<num_params>(format);
    std::array<size_t, num_params> param_sizes;
    GetParamSizes(param_types, param_sizes, 10, 3.1415, 9.618, "Hello World",
                  "This is ptr.", 23);
    REQUIRE(param_sizes == std::array<size_t, num_params>{
                               sizeof(10), sizeof(3.1415), sizeof(9.618), 0,
                               sizeof(void *), sizeof(23)});
}

TEST_CASE("AsSizet", "[AsSizet]") {
    REQUIRE(AsSizet(114514LL) == static_cast<uint64_t>(114514ULL));
    REQUIRE(AsSizet(3.1415) == static_cast<uint64_t>(0));
}

TEST_CASE("Floating points with dynamic precision", "[GetArgSizes]") {
    constexpr char format[] = "Hello %.*lf, %lu, %*d World!!!";
    constexpr auto num_params = FormatParametersCount(format);
    constexpr auto param_types = AnalyzeFormatParameters<num_params>(format);
    REQUIRE(param_types.size() == 5);
    size_t string_sizes[num_params + 1];
    size_t pre_precision = 0;
    size_t args_size = GetArgSizes(param_types, string_sizes, pre_precision, 25,
                                   3.1415, 32, 28, 10);
    REQUIRE(args_size ==
            sizeof(25) + sizeof(3.1415) + sizeof(32) + sizeof(28) + sizeof(10));
}

TEST_CASE("Args with char*", "[GetArgSizes]") {
    constexpr char format[] = "Hello %.*lf, %*.*s World!!!";
    constexpr auto num_params = FormatParametersCount(format);
    constexpr auto param_types = AnalyzeFormatParameters<num_params>(format);
    char str[] = "A random string";
    REQUIRE(param_types.size() == 5);
    size_t string_sizes[num_params + 1];
    size_t pre_precision = 0;
    size_t args_size = GetArgSizes(param_types, string_sizes, pre_precision, 25,
                                   3.1415, 32, 28, str);
    REQUIRE(args_size == sizeof(25) + sizeof(3.1415) + sizeof(32) + sizeof(28) +
                             sizeof(size_t) + sizeof(str) - 1 + 1);
    REQUIRE(string_sizes[4] == sizeof(str) - 1);
}

TEST_CASE("Args with const char*", "[GetArgSizes]") {
    constexpr char format[] = "Hello %.*lf, %*.*s World!!!";
    constexpr auto num_params = FormatParametersCount(format);
    constexpr auto param_types = AnalyzeFormatParameters<num_params>(format);
    REQUIRE(param_types.size() == 5);
    size_t string_sizes[num_params + 1];
    size_t pre_precision = 0;
    size_t args_size = GetArgSizes(param_types, string_sizes, pre_precision, 25,
                                   3.1415, 32, 28, "A random string.");
    REQUIRE(args_size == sizeof(25) + sizeof(3.1415) + sizeof(32) + sizeof(28) +
                             sizeof(size_t) + strlen("A random string.") + 1);
    REQUIRE(string_sizes[4] == strlen("A random string."));
}

TEST_CASE("Args with wchar_t*", "[GetArgSizes]") {
    constexpr char format[] = "Hello %.*lf, %*.*ls World!!!";
    constexpr auto num_params = FormatParametersCount(format);
    constexpr auto param_types = AnalyzeFormatParameters<num_params>(format);
    wchar_t str[] = L"A random string.";
    REQUIRE(param_types.size() == 5);
    size_t string_sizes[num_params + 1];
    size_t pre_precision = 0;
    size_t args_size = GetArgSizes(param_types, string_sizes, pre_precision, 25,
                                   3.1415, 32, 28, str);
    REQUIRE(args_size == sizeof(25) + sizeof(3.1415) + sizeof(32) + sizeof(28) +
                             sizeof(size_t) + wcslen(str) * sizeof(wchar_t) +
                             1);
    REQUIRE(string_sizes[4] == wcslen(str) * sizeof(wchar_t));
}

TEST_CASE("Args with const wchar_t*", "[GetArgSizes]") {
    constexpr char format[] = "Hello %.*lf, %*.*ls World!!!";
    constexpr auto num_params = FormatParametersCount(format);
    constexpr auto param_types = AnalyzeFormatParameters<num_params>(format);
    REQUIRE(param_types.size() == 5);
    size_t string_sizes[num_params + 1];
    size_t pre_precision = 0;
    size_t args_size = GetArgSizes(param_types, string_sizes, pre_precision, 25,
                                   3.1415, 32, 28, L"A random string.");
    REQUIRE(args_size == sizeof(25) + sizeof(3.1415) + sizeof(32) + sizeof(28) +
                             sizeof(size_t) +
                             wcslen(L"A random string.") * sizeof(wchar_t) + 1);
    REQUIRE(string_sizes[4] == wcslen(L"A random string.") * sizeof(wchar_t));
}