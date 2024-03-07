#ifndef OLOG_OLOG_H
#define OLOG_OLOG_H

#include <array>
#include <cstdio>

#include "log_info.h"
#include "logger.h"
#include "portability.h"
#include "utils.h"

namespace olog {

template <size_t _FormatLength, size_t _NumParams, size_t _NumConversions,
          typename... _Args>
inline void Log(int& log_id, const char* filename, const int line_num,
                log_info::LogLevel severity, const char (&fmt)[_FormatLength],
                const char* conversion_storage,
                const std::array<log_info::FormatFragment, _NumConversions>&
                    format_fragments,
                const std::array<log_info::ParamType, _NumParams>& param_types,
                _Args... args) {
    static_assert(
        _NumParams == sizeof...(args),
        "The number of parameters is different from the number of arguments");

    /*比当前日志等级高则直接退出。*/
    if (severity > logger::Logger::GetLogLevel())
        return;

    // 向 Logger 注册该日志的静态信息。
    if (log_id == log_info::UNREGISTERED_LOG_ID) {
        static std::array<size_t, _NumParams> param_sizes;
        log_info::GetParamSizes(param_types, param_sizes, args...);

        log_info::StaticLogInfo info(
            filename, line_num, severity, _FormatLength, _NumConversions,
            _NumParams, fmt, conversion_storage, format_fragments.data(),
            param_types.data(), param_sizes.data());
        logger::Logger::RegisterLogInfo(info, log_id);
    }

    // 存储实参中字符串的长度（与 strlen 或 wcslen 的计算值相同）的数组。+1
    // 是防止在无参情况下出错。
    size_t string_sizes[_NumParams + 1];

    // 前一个参数作为精度值时，用该变量存储。
    // 该变量用于指示实参中字符串被存储的长度，所以使用 size_t 类型。
    size_t pre_precision = 0;

    int64_t timestamp = utils::GetMsSystemClockInterval();

    size_t alloc_size = log_info::GetArgSizes(param_types, string_sizes,
                                              pre_precision, args...) +
                        sizeof(log_info::DynamicLogInfo);

    // 获取写入位置。
    char* write_pos = logger::Logger::ReserveAlloc(alloc_size);
#ifndef NDEBUG
    char* original_write_pos = write_pos;
#endif

    // 写入日志的动态信息头部。
    log_info::DynamicLogInfo* dynamic_info =
        new (write_pos) log_info::DynamicLogInfo();
    write_pos += sizeof(log_info::DynamicLogInfo);
    dynamic_info->log_id_ = log_id;
    dynamic_info->info_size_ = alloc_size;
    dynamic_info->ms_timestamp_ = timestamp;

    // 写入实参。
    size_t args_size =
        log_info::StoreArguments(write_pos, param_types, string_sizes, args...);

#ifndef NDEBUG
    assert(static_cast<size_t>(write_pos - original_write_pos) == alloc_size);
#endif

    // 结束写入，更新缓冲区。
    logger::Logger::FinishAlloc(alloc_size);
}

/**
 * @brief
 * 检查传入的参数是否符合 printf 格式。
 */
static void OLOG_PRINTF_FORMAT_ATTR(1, 2)
    CheckFormat(OLOG_PRINTF_FORMAT const char* format, ...){};

}  // namespace olog

/*
 * 如同 INFO、DEBUG 之类的宏被过多的使用，
 * 为了防止出现问题，OLog 对日志等级不使用宏定义。
 */
using LogLevel = olog::log_info::LogLevel;

/**
 * @brief
 *
 */
#define OLOG(severity, format, ...)                                                               \
    do {                                                                                          \
        /* 静态存储格式串，供 Logger 使用。*/                                         \
        static constexpr char format_str[] = format;                                              \
                                                                                                  \
        /* 该条日志所对应的 id。静态存储，初始化为 UNREGISTERED，在经 Looger \
         * 注册后分配一个唯一值。*/                                                    \
        static int log_id = olog::log_info::UNREGISTERED_LOG_ID;                                  \
                                                                                                  \
        /* 格式串所需参数数量。 */                                                      \
        constexpr size_t num_parameters =                                                         \
            olog::log_info::FormatParametersCount(format_str);                                    \
                                                                                                  \
        /* 格式串中格式描述符数量。*/                                                 \
        constexpr size_t num_conversions =                                                        \
            olog::log_info::ConversionSpecifiersCount(format_str);                                \
                                                                                                  \
        /* 存储格式描述符所需数组大小。*/                                           \
        constexpr size_t conversion_storage_size =                                                \
            olog::log_info::SizeConversionStorageNeeds(format_str);                               \
                                                                                                  \
        /* 描述格式串所需参数类型的数组。静态存储，供 Logger 使用。*/     \
        static constexpr std::array<olog::log_info::ParamType, num_parameters>                    \
            param_types =                                                                         \
                olog::log_info::AnalyzeFormatParameters<num_parameters>(                          \
                    format_str);                                                                  \
                                                                                                  \
        /* 生成存储格式描述符的数组。静态存储，供 Logger 使用。*/           \
        static constexpr std::array<char, conversion_storage_size>                                \
            conversion_storage = olog::log_info::MakeConversionStorage<                           \
                conversion_storage_size>(format_str);                                             \
        /* 格式串中格式描述符片段的数组。静态存储，供 Logger 使用。*/     \
        static constexpr std::array<olog::log_info::FormatFragment,                               \
                                    num_conversions>                                              \
            format_fragments =                                                                    \
                olog::log_info::GetFormatFragments<num_conversions>(                              \
                    format_str, conversion_storage);                                              \
                                                                                                  \
        /* 对参数进行检查。使用 if(false){...}                                          \
         * 防止对传入的参数进行求值，例如 i++。 */                                \
        if (false) {                                                                              \
            olog::CheckFormat(format, ##__VA_ARGS__);                                             \
        }                                                                                         \
                                                                                                  \
        olog::Log(log_id, __FILE__, __LINE__, severity, format_str,                               \
                  conversion_storage.data(), format_fragments, param_types,                       \
                  ##__VA_ARGS__);                                                                 \
    } while (false)

#endif