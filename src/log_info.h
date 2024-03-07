#ifndef OLOG_LOG_INFO_H
#define OLOG_LOG_INFO_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace olog {

/**
 * log_info 命名空间下定义了日志的静态信息和动态信息。
 * 以及一些对日志信息进行操作的相关函数。
 */
namespace log_info {

// 未注册的 log_id 的默认值。
static constexpr int UNREGISTERED_LOG_ID = -1;

enum class LogLevel : uint8_t {
    NONE = 0,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    NUMBER_OF_LOG_LEVELS  // 日志级别的数量。
};

enum class ParamType : int32_t {
    // 非法类型。
    INVALID = -6,

    // 作为动态宽度的参数类型。例如 "%*.d" 中的 '*'。
    DYNAMIC_WIDTH = -5,

    // 作为动态精度的参数类型。例如 "%.*lf" 中的 '*'。
    DYNAMIC_PRECISION = -4,

    // 作为非字符串的参数类型。例如 "%d"。
    NON_STRING = -3,

    // 作为有动态精度的字符串的参数类型。例如 "%.*s"。
    STRING_WITH_DYNAMIC_PRECISION = -2,

    // 作为没有精度的字符串的参数类型。例如 "%s"。
    STRING_WITH_NO_PRECISION = -1,

    // 作为有精度的字符串的参数类型。例如 "%.20s"。
    STRING = 0
};

/**
 * @brief
 * 格式串中格式指示符所指定的数据类型。
 */
enum class ConversionType : uint8_t {
    NONE,

    unsigned_char_t,
    unsigned_short_int_t,
    unsigned_int_t,
    unsigned_long_int_t,
    unsigned_long_long_int_t,
    uintmax_t_t,
    size_t_t,
    wint_t_t,

    signed_char_t,
    short_int_t,
    int_t,
    long_int_t,
    long_long_int_t,
    intmax_t_t,
    ptrdiff_t_t,

    double_t,
    long_double_t,

    const_void_ptr_t,
    const_char_ptr_t,
    const_wchar_t_ptr_t,

    MAX_CONVERSION_TYPE
};

/**
 * @brief
 * 格式描述符片段。
 * 它提供格式描述符的类型、在格式串中的位置，以及可以用于直接供 printf
 * 族函数使用的格式描述符字符串副本。
 */
struct FormatFragment {
    // 格式描述符所描述的类型。
    const ConversionType conversion_type_;

    // 格式描述符在格式串中所占的长度。
    // 例如在 "int %.*lf" 中，格式描述符 "%.*lf" 占 5 位。
    const size_t specifier_length_;

    // 该片段在格式串中的位置。
    const size_t format_pos_;

    // 在格式描述符尾部添加了 '\0' 后的存储数组中的起始位置。
    const size_t storage_pos_;

    friend bool operator==(const FormatFragment&, const FormatFragment&);
};

/**
 * @brief
 * 比较两个 FormatFragment 是否相等。
 * （用于测试的函数。）
 *
 * @param f1
 * @param f2
 * @return true
 * @return false
 */
bool operator==(const FormatFragment& f1, const FormatFragment& f2);

struct StaticLogInfo {
    explicit StaticLogInfo(const char* filename, const uint32_t line_number,
                           const LogLevel log_level, const size_t format_len,
                           const size_t num_conversions,
                           const size_t num_parameters, const char* format_str,
                           const char* conversion_storage,
                           const FormatFragment* format_fragments,
                           const ParamType* param_types,
                           const size_t* param_sizes)
        : filename_(filename),
          line_number_(line_number),
          log_level_(log_level),
          format_len_(format_len),
          num_conversions_(num_conversions),
          num_parameters_(num_parameters),
          format_str_(format_str),
          conversion_storage_(conversion_storage),
          format_fragments_(format_fragments),
          param_types_(param_types),
          param_sizes_(param_sizes) {}

    // 日志所在的文件名。
    const char* filename_;

    // 日志在文件中的行号。
    const uint32_t line_number_;

    // 日志等级。
    const LogLevel log_level_;

    // 格式串的长度。
    const size_t format_len_;

    // 格式串中格式描述符的数量。
    const size_t num_conversions_;

    // 格式串需要的参数数量。
    const size_t num_parameters_;

    // 指向格式串的指针。
    const char* format_str_;

    // 指向存储格式描述符的数组的指针。
    const char* conversion_storage_;

    // 指向存储格式描述符片段的数组。
    const FormatFragment* format_fragments_;

    // 指向格式串所需参数类型的数组。
    const ParamType* param_types_;

    // 指向参数所占大小的数组。
    const size_t* param_sizes_;
};

struct DynamicLogInfo {
    // 静态信息所对应的 id。
    size_t log_id_;

    // 包含 arg_data 在内的整个结构体的大小。
    size_t info_size_;

    // 单位为毫秒的时间戳。
    int64_t ms_timestamp_;

    // 使用柔性数组传递格式串的实参信息。
    char arg_data[];
};

/**
 * @brief
 * LogAssembler 的职责是通过静态信息和动态信息将日志恢复并写入到指定位置。
 * 关于被写入的缓冲区，LogAssembler
 * 采用很保守的策略以避免缓冲区越界。当写入的日志字节数使得写指针到达了指定的缓冲区结尾时，无论写入的内容是否完整，LogAssembler
 * 都会回退写指针，并报告缓冲区已满。LogAssembler
 * 会将一条日志分批写入，防止回退过多已写入字节。
 */
class LogAssembler {
  public:
    LogAssembler();

    ~LogAssembler() = default;

    LogAssembler(const LogAssembler&) = delete;

    LogAssembler(LogAssembler&&) = delete;

    inline void setBuffer(char* write_pos, size_t buffer_size) {
        write_pos_ = write_pos;
        buffer_size_ = buffer_size;
        writed_count_ = 0;
    }

    inline void loadLogInfo(const StaticLogInfo* static_info,
                            const DynamicLogInfo* dynamic_info,
                            size_t producer_id) {
        loadStaticInfo(static_info);
        loadDynamicInfo(dynamic_info);
        setProducerId(producer_id);
        resetIndices();
        resetFlags();
    }

    /**
     * @brief 进行一次写入操作。
     *
     * @return 写入缓冲区的字节数。
     *
     * @note noexcept 调用前需保证静态信息和动态信息已装载。
     */
    size_t write() noexcept;

    inline bool hasRemainingData() const {
        if (static_log_info_ == nullptr || dynamic_log_info_ == nullptr)
            return false;
        return !is_end_of_log_writed_;
    }

    inline size_t getWritedBytes() const { return writed_count_; }

    inline size_t getFreeBytes() const { return buffer_size_ - writed_count_; }

    inline bool isBufferFull() const { return is_full_; }

  private:
    /**
     * @brief 装载日志静态信息。
     *
     * @param static_info
     *
     * @return 前一个被装载的日志静态信息。
     */
    const StaticLogInfo* loadStaticInfo(const StaticLogInfo* static_info);

    /**
     * @brief 装载日志动态信息。
     *
     * @param dynamic_info
     *
     * @return 前一个被装载的日志动态信息。
     */
    const DynamicLogInfo* loadDynamicInfo(const DynamicLogInfo* dynamic_info);

    /**
     * @brief 设置日志来自的生产者的编号。默认为 0。
     *
     * @param id
     */
    void setProducerId(size_t id);

    inline void resetIndices() {
        conversion_index_ = 0;
        parameter_index_ = 0;
        format_index_ = 0;
    }

    inline void resetFlags() {
        is_timestamp_writed_ = is_filename_and_linenum_writed_ =
            is_severity_writed_ = is_producer_id_writed_ =
                is_end_of_log_writed_ = false;
    }

    inline void finishWriting(size_t bytes_writed) {
        bytes_last_writed_ += bytes_writed;
        writed_count_ += bytes_writed;
        write_pos_ += bytes_writed;
    }

    /**
     * @brief
     * 内部的 write() 的辅助方法。
     * 尝试将字符串写入缓冲区。
     *
     * @param src 指向写入的字符串。
     * @param len 写入长度。
     * @return 写入的字节数。
     * 当返回值与 len 参数相等时代表写入成功，否则返回 0 代表失败，同时设置
     * is_full_ 标志。
     */
    inline size_t tryToWriteAStringToBuffer(const char* src,
                                            size_t len) noexcept {
        if (is_full_)
            return 0;
        if (len >= getFreeBytes()) {
            is_full_ = true;
            return 0;
        }
        memcpy(write_pos_, src, len);
        return len;
    }

    template <typename _ArgTp>
    inline size_t tryToWriteArgToBuffer(const char* fmt, int width,
                                        int precision, _ArgTp arg) {
        size_t bytes_writed = 0;
        if (width == -1 && precision == -1)
            bytes_writed += snprintf(write_pos_, getFreeBytes(), fmt, arg);
        else if (width != -1 && precision == -1)
            bytes_writed +=
                snprintf(write_pos_, getFreeBytes(), fmt, width, arg);
        else if (width == -1 && precision != -1)
            bytes_writed +=
                snprintf(write_pos_, getFreeBytes(), fmt, precision, arg);
        else
            bytes_writed += snprintf(write_pos_, getFreeBytes(), fmt, width,
                                     precision, arg);
        if (bytes_writed >= getFreeBytes()) {
            is_full_ = true;
            return 0;
        }
        return bytes_writed;
    }

  private:
    char* write_pos_;
    size_t buffer_size_;

    // 向缓冲区已经写入的字节总数。
    size_t writed_count_;

    // 上一次调用 write() 写入的字节数。
    size_t bytes_last_writed_;

    size_t conversion_index_;
    size_t parameter_index_;
    size_t format_index_;
    const StaticLogInfo* static_log_info_;
    const DynamicLogInfo* dynamic_log_info_;
    const char* args_read_pos_;

    // 时间戳字符串，尾部有一个空格。
    std::array<char, std::size("YYYY-MM-DD hh:mm:ss.mil ")> timestamp_str_;

    // 文件名与行号，中间以 ':' 分隔，尾部有一个空格。
    // "filename:linenum "
    std::string filename_and_linenum_;

    // 日志来自的生产者编号，被方括号包裹，尾部跟有 ": "。
    std::string producer_id_;

    std::string_view end_of_log_;

    // 指示写缓冲区是否已满。
    bool is_full_;

    // 以下标志用于在写缓冲区满时，write() 方法
    // 被中断后，再次调用 write() 方法可以恢复现场。

    bool is_timestamp_writed_;
    bool is_filename_and_linenum_writed_;
    bool is_severity_writed_;
    bool is_producer_id_writed_;
    bool is_end_of_log_writed_;
};

constexpr inline bool IsConversionSpecifier(char c) {
    return c == 'd' || c == 'i' || c == 'u' || c == 'o' || c == 'x' ||
           c == 'X' || c == 'f' || c == 'F' || c == 'e' || c == 'E' ||
           c == 'g' || c == 'G' || c == 'a' || c == 'A' || c == 'c' ||
           c == 'p' || c == '%' || c == 's' || c == 'n';
}

constexpr inline bool IsFlag(char c) {
    return c == '-' || c == '+' || c == ' ' || c == '#' || c == '0';
}

constexpr inline bool IsLength(char c) {
    return c == 'h' || c == 'l' || c == 'j' || c == 'z' || c == 't' || c == 'L';
}

constexpr inline bool IsDigit(char c) { return (c >= '0' && c <= '9'); }

/**
 * @brief
 * 解析目标格式串中指定位置的所需参数信息。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @param param_num
 * 被解析的参数的位置。
 *
 * @return constexpr ParamType
 *
 * @throw std::invalid_argument
 * 对参数解析错误。
 */
template <size_t _FormatLength>
constexpr inline ParamType GetParamInfo(const char (&fmt)[_FormatLength],
                                        size_t param_num = 0) {
    size_t index = 0;
    while (index < _FormatLength) {
        // 普通字符，直接略过。
        if (fmt[index] != '%') {
            ++index;
            continue;
        }
        ++index;

        // 连续遇到两个 '%'，转义。
        if (fmt[index] == '%') {
            ++index;
            continue;
        }

        // 处理 flag。
        while (IsFlag(fmt[index])) {
            ++index;
        }

        // 处理 width。
        if (fmt[index] == '*') {
            if (param_num == 0)
                return ParamType::DYNAMIC_WIDTH;
            --param_num;
            ++index;
        } else {
            while (IsDigit(fmt[index])) {
                ++index;
            }
        }

        // 处理 precision。
        bool has_dynamic_precision = false;
        int precision = -1;
        if (fmt[index] == '.') {
            ++index;

            if (fmt[index] == '*') {
                if (param_num == 0)
                    return ParamType::DYNAMIC_PRECISION;
                has_dynamic_precision = true;
                --param_num;
                ++index;
            } else {
                // 计算已指明的精度值。
                precision = 0;
                while (IsDigit(fmt[index])) {
                    precision = precision * 10 + (fmt[index] - '0');
                    ++index;
                }
            }
        }

        // 处理 length。
        while (IsLength(fmt[index])) {
            ++index;
        }

        // 检查 conversion specifier。
        if (!IsConversionSpecifier(fmt[index]))
            throw std::invalid_argument(
                "Unrecognized conversion specifier after %");

        if (fmt[index] == 'n')
            throw std::invalid_argument(
                "Conversion specifier %n is not supported by OLog.");

        if (param_num > 0) {
            --param_num;
            ++index;
            continue;
        }

        if (fmt[index] != 's')
            return ParamType::NON_STRING;
        else if (has_dynamic_precision)
            return ParamType::STRING_WITH_DYNAMIC_PRECISION;
        else if (precision == -1)
            return ParamType::STRING_WITH_NO_PRECISION;
        else
            return ParamType(precision);
    }
    return ParamType::INVALID;
}

/**
 * @brief
 * 计算格式串所需参数的总数。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @return constexpr size_t
 */
template <size_t _FormatLength>
constexpr inline size_t FormatParametersCount(
    const char (&fmt)[_FormatLength]) {
    size_t num_parameters = 0;
    while (GetParamInfo(fmt, num_parameters) != ParamType::INVALID)
        num_parameters++;
    return num_parameters;
}

/**
 * @brief
 * AnalyzeFormatParameters 的辅助函数。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @tparam _Indices
 * 参数所在的下标，该参数会被自动推导。
 *
 * @return constexpr std::array<ParamType, sizeof...(_Indices)>
 */
template <size_t _FormatLength, std::size_t... _Indices>
constexpr std::array<ParamType, sizeof...(_Indices)>
AnalyzeFormatParametersHelper(const char (&fmt)[_FormatLength],
                              std::index_sequence<_Indices...>) {
    return {{GetParamInfo(fmt, _Indices)...}};
}

/**
 * @brief
 * 解析格式串所需的参数类型。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @tparam _NumParam
 * 格式串所需参数数量。
 *
 * @return constexpr std::array<ParamType, _NumParam>
 */
template <size_t _NumParam, size_t _FormatLength>
constexpr inline std::array<ParamType, _NumParam> AnalyzeFormatParameters(
    const char (&fmt)[_FormatLength]) {
    return AnalyzeFormatParametersHelper(fmt,
                                         std::make_index_sequence<_NumParam>());
}

/**
 * @brief 解析目标格式串中指定位置的格式指示符信息。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @param conversion_num
 * 被解析的格式指示符位置。
 *
 * @return constexpr ConversionType
 *
 * @throw std::invalid_argument
 * 对参数解析错误。
 */
template <size_t _FormatLength>
constexpr inline ConversionType GetConversionType(
    const char (&fmt)[_FormatLength], size_t conversion_num = 0) {
    unsigned short h_cnt = 0;  // 对 length 中的 'h' 计数。
    unsigned short l_cnt = 0;  // 对 length 中的 'l' 计数。
    bool L_flag = false;       // length 中是否包含 'L'。
    bool j_flag = false;       // length 中是否包含 'j'。
    bool z_flag = false;       // length 中是否包含 'z'。
    bool t_flag = false;       // length 中是否包含 't'。
    char specifier = 0;        // conversion specifier。

    size_t index = 0;
    while (index < _FormatLength) {
        if (fmt[index] != '%') {
            ++index;
            continue;
        }
        ++index;

        // 连续两个 '%'，转义。
        if (fmt[index] == '%') {
            ++index;
            continue;
        }

        // 处理 flag。
        while (IsFlag(fmt[index])) {
            ++index;
        }

        // 处理 width。
        if (fmt[index] == '*') {
            ++index;
        } else {
            while (IsDigit(fmt[index])) {
                ++index;
            }
        }

        // 处理 precision。
        if (fmt[index] == '.') {
            ++index;
            if (fmt[index] == '*') {
                ++index;
            } else {
                while (IsDigit(fmt[index])) {
                    ++index;
                }
            }
        }

        // 处理 length。
        while (IsLength(fmt[index])) {
            switch (fmt[index]) {
            case 'L':
                L_flag = true;
                break;
            case 'h':
                h_cnt++;
                break;
            case 'j':
                j_flag = true;
                break;
            case 'l':
                l_cnt++;
                break;
            case 't':
                t_flag = true;
                break;
            case 'z':
                z_flag = true;
                break;
            default:
                break;
            }
            ++index;
        }

        if (!IsConversionSpecifier(fmt[index]))
            throw std::invalid_argument(
                "Unrecognized conversion specifier after %");

        if (fmt[index] == 'n')
            throw std::invalid_argument(
                "Conversion specifier %n is not supported by OLog.");

        // 非目标 conversion，略过。
        if (conversion_num > 0) {
            --conversion_num;
            h_cnt = l_cnt = 0;
            L_flag = j_flag = z_flag = t_flag = false;
            ++index;
            continue;
        }

        specifier = fmt[index];
        break;
    }

    /* 分析 ConversionType。*/

    // Signed integers.
    if (specifier == 'd' || specifier == 'i') {
        if (h_cnt >= 2)
            return ConversionType::signed_char_t;
        if (l_cnt >= 2)
            return ConversionType::long_long_int_t;
        if (h_cnt >= 1)
            return ConversionType::short_int_t;
        if (l_cnt >= 1)
            return ConversionType::long_int_t;
        if (j_flag)
            return ConversionType::intmax_t_t;
        if (z_flag)
            return ConversionType::size_t_t;
        if (t_flag)
            return ConversionType::ptrdiff_t_t;
        return ConversionType::int_t;
    }

    // Unsigned integers.
    if (specifier == 'u' || specifier == 'o' || specifier == 'x' ||
        specifier == 'X') {
        if (h_cnt >= 2)
            return ConversionType::unsigned_char_t;
        if (l_cnt >= 2)
            return ConversionType::unsigned_long_long_int_t;
        if (h_cnt >= 1)
            return ConversionType::unsigned_short_int_t;
        if (l_cnt >= 1)
            return ConversionType::unsigned_long_int_t;
        if (j_flag)
            return ConversionType::uintmax_t_t;
        if (z_flag)
            return ConversionType::size_t_t;
        if (t_flag)
            return ConversionType::ptrdiff_t_t;
        return ConversionType::unsigned_int_t;
    }

    // Strings.
    if (specifier == 's') {
        if (l_cnt >= 1)
            return ConversionType::const_wchar_t_ptr_t;
        return ConversionType::const_char_ptr_t;
    }

    // Pointer.
    if (specifier == 'p') {
        return ConversionType::const_void_ptr_t;
    }

    // Floating points.
    if (specifier == 'f' || specifier == 'F' || specifier == 'e' ||
        specifier == 'E' || specifier == 'g' || specifier == 'G' ||
        specifier == 'a' || specifier == 'A') {
        if (L_flag)
            return ConversionType::long_double_t;
        return ConversionType::double_t;
    }

    // Characters.
    if (specifier == 'c') {
        if (l_cnt >= 1)
            return ConversionType::wint_t_t;
        return ConversionType::int_t;
    }

    return ConversionType::NONE;
}

/**
 * @brief 获取格式串中格式指示符的总数。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @return constexpr size_t
 *
 * @throw std::invalid_argument
 * 参数解析错误。
 */
template <size_t _FormatLength>
constexpr inline size_t ConversionSpecifiersCount(
    const char (&fmt)[_FormatLength]) {
    size_t count = 0, index = 0;
    while (index < _FormatLength) {
        if (fmt[index] != '%') {
            ++index;
            continue;
        }
        ++index;

        // 连续两个 '%'，转义。
        if (fmt[index] == '%') {
            ++index;
            continue;
        }

        // 处理 flag。
        while (IsFlag(fmt[index])) {
            ++index;
        }

        // 处理 width。
        if (fmt[index] == '*') {
            ++index;
        } else {
            while (IsDigit(fmt[index])) {
                ++index;
            }
        }

        // 处理 precision。
        if (fmt[index] == '.') {
            ++index;
            if (fmt[index] == '*') {
                ++index;
            } else {
                while (IsDigit(fmt[index])) {
                    ++index;
                }
            }
        }

        // 处理 length。
        while (IsLength(fmt[index])) {
            ++index;
        }

        if (!IsConversionSpecifier(fmt[index]))
            throw std::invalid_argument(
                "Unrecognized conversion specifier after %");

        if (fmt[index] == 'n')
            throw std::invalid_argument(
                "Conversion specifier %n is not supported by OLog.");

        ++count;
        ++index;
    }
    return count;
}

/**
 * @brief
 * 获取存储格式描述符的数组的所需大小。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @return constexpr size_t
 *
 * @throw std::invalid_argument
 * 参数解析错误。
 */
template <size_t _FormatLength>
constexpr inline size_t SizeConversionStorageNeeds(
    const char (&fmt)[_FormatLength]) {
    size_t need_size = 0, count = 0, index = 0;
    while (index < _FormatLength) {
        if (fmt[index] != '%') {
            ++index;
            continue;
        }
        ++index;

        // 连续两个 '%'，转义。
        if (fmt[index] == '%') {
            ++index;
            continue;
        }

        ++need_size;  // 为 '%' 计数。

        // 处理 flag。
        while (IsFlag(fmt[index])) {
            ++index;
            ++need_size;
        }

        // 处理 width。
        if (fmt[index] == '*') {
            ++index;
            ++need_size;
        } else {
            while (IsDigit(fmt[index])) {
                ++index;
                ++need_size;
            }
        }

        // 处理 precision。
        if (fmt[index] == '.') {
            ++index;
            ++need_size;  // 为 '.' 计数。
            if (fmt[index] == '*') {
                ++index;
                ++need_size;
            } else {
                while (IsDigit(fmt[index])) {
                    ++index;
                    ++need_size;
                }
            }
        }

        // 处理 length。
        while (IsLength(fmt[index])) {
            ++index;
            ++need_size;
        }

        if (!IsConversionSpecifier(fmt[index]))
            throw std::invalid_argument(
                "Unrecognized conversion specifier after %");

        if (fmt[index] == 'n')
            throw std::invalid_argument(
                "Conversion specifier %n is not supported by OLog.");

        ++count;
        ++need_size;
        ++index;
    }
    return need_size + count;
}

/**
 * @brief
 * 获取格式描述符中的各个字符。
 * 该函数由 MakeConversionStorageHelper 调用以生成格式描述符数组。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @param num
 * 字符在存储数组中的位置。
 *
 * @return constexpr char
 *
 * @throw std::invalid_argument
 * 参数解析错误。
 */
template <size_t _FormatLength>
constexpr inline char GetConversionSpecifierChar(
    const char (&fmt)[_FormatLength], size_t num) {
    size_t index = 0;
    while (index < _FormatLength) {
        if (fmt[index] != '%') {
            ++index;
            continue;
        }
        ++index;

        // 连续两个 '%'，转义。
        if (fmt[index] == '%') {
            ++index;
            continue;
        }

        // 处理 '%'。
        if (num == 0)
            return '%';
        --num;

        // 处理 flag。
        while (IsFlag(fmt[index])) {
            if (num == 0)
                return fmt[index];
            --num;
            ++index;
        }

        // 处理 width。
        if (fmt[index] == '*') {
            if (num == 0)
                return '*';
            --num;
            ++index;
        } else {
            while (IsDigit(fmt[index])) {
                if (num == 0)
                    return fmt[index];
                --num;
                ++index;
            }
        }

        // 处理 precision。
        if (fmt[index] == '.') {
            if (num == 0)
                return fmt[index];
            --num;
            ++index;
            if (fmt[index] == '*') {
                if (num == 0)
                    return '*';
                --num;
                ++index;
            } else {
                while (IsDigit(fmt[index])) {
                    if (num == 0)
                        return fmt[index];
                    --num;
                    ++index;
                }
            }
        }

        // 处理 length。
        while (IsLength(fmt[index])) {
            if (num == 0)
                return fmt[index];
            --num;
            ++index;
        }

        if (!IsConversionSpecifier(fmt[index]))
            throw std::invalid_argument(
                "Unrecognized conversion specifier after %");

        if (fmt[index] == 'n')
            throw std::invalid_argument(
                "Conversion specifier %n is not supported by OLog.");

        if (num == 0)
            return fmt[index];
        --num;
        if (num == 0)
            return '\0';  // 插入 '\0' 作为结尾。
        --num;
        ++index;
    }

    return '\0';
}

/**
 * @brief
 * MakeConversionStorage 的辅助函数。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @tparam _Indices
 * 下标序列。
 *
 * @return constexpr std::array<char, sizeof...(_Indices)>
 */
template <size_t _FormatLength, std::size_t... _Indices>
constexpr inline std::array<char, sizeof...(_Indices)>
MakeConversionStorageHelper(const char (&fmt)[_FormatLength],
                            std::index_sequence<_Indices...>) {
    return {{GetConversionSpecifierChar(fmt, _Indices)...}};
}

/**
 * @brief
 * 生成包含格式描述符的字符数组。
 * 其中不同格式描述符之间以 '\0' 分隔。
 *
 * @tparam _StorageSize
 * 数组大小。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @return constexpr std::array<char, _StorageSize>
 */
template <size_t _StorageSize, size_t _FormatLength>
constexpr inline std::array<char, _StorageSize> MakeConversionStorage(
    const char (&fmt)[_FormatLength]) {
    return MakeConversionStorageHelper(
        fmt, std::make_index_sequence<_StorageSize>());
}

/**
 * @brief
 * 返回指定格式描述符所在的起始位置。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @param num
 * 指定第 num 个格式描述符。
 *
 * @return constexpr size_t
 *
 * @throw std::invalid_argument
 * 参数解析错误。
 */
template <size_t _FormatLength>
constexpr inline size_t GetConversionSpecifierPosition(
    const char (&fmt)[_FormatLength], size_t num) {
    size_t index = 0;
    while (index < _FormatLength) {
        if (fmt[index] != '%') {
            ++index;
            continue;
        }
        ++index;

        // 连续两个 '%'，转义。
        if (fmt[index] == '%') {
            ++index;
            continue;
        }

        // 格式描述串，如果是目标位置则返回。
        if (num == 0)
            return index - 1;
        --num;

        // 处理 flag。
        while (IsFlag(fmt[index])) {
            ++index;
        }

        // 处理 width。
        if (fmt[index] == '*') {
            ++index;
        } else {
            while (IsDigit(fmt[index])) {
                ++index;
            }
        }

        // 处理 precision。
        if (fmt[index] == '.') {
            ++index;
            if (fmt[index] == '*') {
                ++index;
            } else {
                while (IsDigit(fmt[index])) {
                    ++index;
                }
            }
        }

        // 处理 length。
        while (IsLength(fmt[index])) {
            ++index;
        }

        if (!IsConversionSpecifier(fmt[index]))
            throw std::invalid_argument(
                "Unrecognized conversion specifier after %");

        if (fmt[index] == 'n')
            throw std::invalid_argument(
                "Conversion specifier %n is not supported by OLog.");

        ++index;
    }

    return index;
}

template <size_t _StorageSize>
constexpr inline size_t GetConversionSpecifierLength(
    const std::array<char, _StorageSize>& storage, size_t num) {
    size_t index = 0, len = 0;
    while (index < _StorageSize) {
        if (storage.at(index) == '\0') {
            if (num == 0)
                return len;
            --num;
            len = 0;
            ++index;
            continue;
        }
        ++len;
        ++index;
    }
    return len;
}

/**
 * @brief
 * 返回指定格式描述符在存储数组中所在的起始位置。
 *
 * @tparam _StorageSize
 * 存储数组的长度，该参数会被自动推导。
 *
 * @param storage
 * 存储格式描述符的数组。
 *
 * @param num
 * 指定第 num 个格式描述符。
 *
 * @return constexpr size_t
 */
template <size_t _StorageSize>
constexpr inline size_t GetConversionSpecifierPositionInStorage(
    const std::array<char, _StorageSize>& storage, size_t num) {
    size_t index = 0;
    while (index < _StorageSize) {
        if (storage[index] != '%') {
            ++index;
            continue;
        }

        if (num == 0)
            return index;
        --num;
        ++index;
    }
    return index;
}

/**
 * @brief
 * GetFormatFragments 的辅助函数。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @tparam _StorageSize
 * 存储数组的长度，该参数会被自动推导。
 *
 * @tparam _Indices
 * 下标序列。
 *
 * @param storage
 * 存储格式描述符的数组。
 *
 * @return constexpr std::array<FormatFragment, sizeof...(_Indices)>
 */
template <size_t _FormatLength, size_t _StorageSize, std::size_t... _Indices>
constexpr inline std::array<FormatFragment, sizeof...(_Indices)>
GetFormatFragmentsHelper(const char (&fmt)[_FormatLength],
                         const std::array<char, _StorageSize>& storage,
                         std::index_sequence<_Indices...>) {
    return {{FormatFragment{
        GetConversionType(fmt, _Indices),
        GetConversionSpecifierLength(storage, _Indices),
        GetConversionSpecifierPosition(fmt, _Indices),
        GetConversionSpecifierPositionInStorage(storage, _Indices)}...}};
}

/**
 * @brief
 * 获取格式串的 FormatFragments 数组。
 *
 * @tparam _NumFormatFragments
 * 数组长度，该参数与格式描述符数量相等。
 *
 * @tparam _StorageSize
 * 存储格式描述符的数组长度。
 *
 * @tparam _FormatLength
 * 格式串长度，该参数会被自动推导。
 *
 * @param storage
 * 存储格式描述符的数组。
 *
 * @return constexpr std::array<FormatFragment, _NumFormatFragments>
 */
template <size_t _NumFormatFragments, size_t _StorageSize, size_t _FormatLength>
constexpr inline std::array<FormatFragment, _NumFormatFragments>
GetFormatFragments(const char (&fmt)[_FormatLength],
                   const std::array<char, _StorageSize>& storage) {
    return GetFormatFragmentsHelper(
        fmt, storage, std::make_index_sequence<_NumFormatFragments>());
}

template <typename _Tp>
constexpr typename std::enable_if<std::is_same<_Tp, char*>::value ||
                                      std::is_same<_Tp, const char*>::value ||
                                      std::is_same<_Tp, wchar_t*>::value ||
                                      std::is_same<_Tp, const wchar_t*>::value,
                                  size_t>::type
GetParamSize(const ParamType& param_type, _Tp arg) {
    if (param_type <= ParamType::NON_STRING)
        return GetParamSize<const void*>(param_type, arg);
    return 0;
}

template <typename _Tp>
constexpr typename std::enable_if<!std::is_same<_Tp, char*>::value &&
                                      !std::is_same<_Tp, const char*>::value &&
                                      !std::is_same<_Tp, wchar_t*>::value &&
                                      !std::is_same<_Tp, const wchar_t*>::value,
                                  size_t>::type
GetParamSize(const ParamType& param_type, _Tp arg) {
    return sizeof(_Tp);
}

/**
 * @brief
 * 获取传给格式串的实参中除了非字符串类型大小数组。
 * 例如对于 ("%d %s\n", getInt(), getString().c_str()) 的实参
 * 会返回 [sizeof(int), 0]。
 *
 * @tparam _NumParam
 * @tparam _Args
 * @param args
 * @return std::array<size_t, _NumParam>
 */
// template <size_t _NumParam, typename... _Args>
// std::array<size_t, _NumParam> GetParamSizes(_Args... args) {
//     return {{GetParamSize(args)...}};
// }

template <size_t _Indices = 0, size_t _NumParam, typename _FirstArg,
          typename... _RestArgs>
void GetParamSizes(const std::array<ParamType, _NumParam>& param_types,
                   std::array<size_t, _NumParam>& param_sizes, _FirstArg first,
                   _RestArgs... rest) {
    param_sizes[_Indices] = GetParamSize(param_types[_Indices], first);
    GetParamSizes<_Indices + 1>(param_types, param_sizes, rest...);
}

/**
 * @brief
 * GetParamSizes 的无参类型。
 *
 * @tparam _Indices
 * @tparam _NumParam
 * @param param_types
 * @param param_sizes
 */
template <size_t _Indices = 0, size_t _NumParam>
void GetParamSizes(const std::array<ParamType, _NumParam>& param_types,
                   std::array<size_t, _NumParam>& param_sizes) {
    return;
}

/**
 * @brief
 * 将不同类型转化为 size_t
 * 该模板函数是在能将类型转换为 size_t 的特化。
 *
 * @tparam _Tp 数据类型。
 * @param val 被转换的数据。
 * @return size_t
 */
template <typename _Tp>
inline typename std::enable_if<std::is_convertible<_Tp, size_t>::value &&
                                   !std::is_floating_point<_Tp>::value,
                               size_t>::type
AsSizet(_Tp val) {
    return val;
}

/**
 * @brief
 * 将不同类型转化为 size_t
 * 该模板函数是不能将类型转换为 size_t 的特化。
 *
 * @tparam _Tp 数据类型。
 * @param val 被转换的数据。
 * @return 0
 */
template <typename _Tp>
inline typename std::enable_if<!std::is_convertible<_Tp, size_t>::value ||
                                   std::is_floating_point<_Tp>::value,
                               size_t>::type
AsSizet(_Tp val) {
    return 0;
}

/**
 * @brief
 * 获取实参占用大小。
 *
 * @tparam _Tp 数据类型。
 * @param param_type 格式串中参数类型。
 * @param string_size
 * 对字符串长度的存储位置，在该函数中没有作用，只是为了方便 GetArgSizes
 * 包展开调用。
 * @param pre_precision 前一个作为精度的实参的值。
 * @param val 实参的值。
 * @return 该实参类型的 sizeof 运算值。
 */
template <typename _Tp>
inline typename std::enable_if<!std::is_same<_Tp, char*>::value &&
                                   !std::is_same<_Tp, const char*>::value &&
                                   !std::is_same<_Tp, wchar_t*>::value &&
                                   !std::is_same<_Tp, const wchar_t*>::value &&
                                   !std::is_same<_Tp, void*>::value &&
                                   !std::is_same<_Tp, const void*>::value,
                               size_t>::type
GetArgSize(const ParamType& param_type, size_t& string_size,
           size_t& pre_precision, _Tp val) {
    if (param_type == ParamType::DYNAMIC_PRECISION)
        // 浮点数作为精度值时会出现不可控的问题，所以需要一个特殊的转换函数来处理。
        pre_precision = AsSizet(val);
    return sizeof(_Tp);
}

/**
 * @brief
 * 获取 char 字符串的占用大小。
 * 该函数是 GetArgSize 的特化。
 *
 * @param param_type 格式串中参数类型。
 * @param string_size 对字符串长度的存储位置。
 * @param pre_precision 前一个作为精度的实参的值。
 * @param str 指向字符串的指针。
 * @return char 字符串（包含 '\0'）占用的大小。
 */
inline size_t GetArgSize(const ParamType& param_type, size_t& string_size,
                         size_t& pre_precision, const char* str) {
    // 当显式声明该实参不作为字符串时，将其当作指针处理。
    if (param_type <= ParamType::NON_STRING)
        return sizeof(void*);

    string_size = strlen(str);
    size_t format_len =
        static_cast<size_t>(param_type);  // 参数中显式指定的字符串长度。

    // 当参数中显式指定长度时，对字符串进行截断。
    if (param_type >= ParamType::STRING && string_size > format_len)
        string_size = format_len;

    // 参数指定为动态精度时，对字符串进行截断。
    else if (param_type == ParamType::STRING_WITH_DYNAMIC_PRECISION &&
             string_size > pre_precision)
        string_size = pre_precision;

    // +1 代表 '\0'。
    return string_size + sizeof(size_t) + 1;
}

/**
 * @brief
 * 获取 wchar 字符串的占用大小。
 * 该函数是 GetArgSize 的特化。
 *
 * @param param_type 格式串中参数类型。
 * @param string_size 对字符串长度的存储位置。
 * @param pre_precision 前一个作为精度的实参的值。
 * @param wstr 指向字符串的指针。
 * @return wchar_t 字符串（包含 '\0'）占用的大小。
 */
inline size_t GetArgSize(const ParamType& param_type, size_t& string_size,
                         size_t& pre_precision, const wchar_t* wstr) {
    // 当显式声明该实参不作为字符串时，将其当作指针处理。
    if (param_type <= ParamType::NON_STRING)
        return sizeof(void*);

    string_size = wcslen(wstr);
    size_t format_len =
        static_cast<size_t>(param_type);  // 参数中显式指定的字符串长度。

    // 当参数中显式指定长度时，对字符串进行截断。
    if (param_type >= ParamType::STRING && string_size > format_len)
        string_size = format_len;

    // 参数指定为动态精度时，对字符串进行截断。
    else if (param_type == ParamType::STRING_WITH_DYNAMIC_PRECISION &&
             string_size > pre_precision)
        string_size = pre_precision;

    string_size *= sizeof(wchar_t);

    // +1 代表 '\0'。
    return string_size + sizeof(size_t) + 1;
}

/**
 * @brief
 * 获取指针的占用大小。
 * 该函数是 GetArgSize 的特化。
 *
 * @param param_type 格式串中参数类型。
 * @param string_size 对字符串长度的存储位置。
 * @param pre_precision 前一个作为精度的实参的值。
 * @param ptr 实参指针的值。
 * @return sizeof(void*)
 */
inline size_t GetArgSize(const ParamType& param_type, size_t& string_size,
                         size_t& pre_precision, const void* ptr) {
    return sizeof(void*);
}

/**
 * @brief
 * 获取存储传给格式串的实参所需的空间大小。
 * 对于非字符串的类型，其需要的存储空间大小就是它的类型大小。
 * 字符串存储的结构为 length(size_t) + char[] + '\0'，
 * 所以它需要的空间大小为 sizeof(size_t) + strlen(str) + 1。
 * 宽字符串则为 sizeof(size_t) + wcslen(str) * sizeof(wchar_t) + 1。
 *
 * @tparam _Index 遍历参数的索引，内部参数，从 0 开始。
 * @tparam _NumArgs 实参的数量。
 * @tparam _StorageSize 存储字符串长度的数组大小。
 * @tparam _FirstArg 第一个实参。
 * @tparam _RestArgs 未展开的剩余实参。
 *
 * @param param_types 描述实参在格式串的参数类型。
 * @param pre_precision 前一个作为精度的值。
 * @param first_arg 第一个实参。
 * @param rest 剩余实参。
 * @return 需要存储实参所需的总空间大小。
 */
template <size_t _Index = 0, size_t _NumArgs, size_t _StorageSize,
          typename _FirstArg, typename... _RestArgs>
inline size_t GetArgSizes(const std::array<ParamType, _NumArgs>& param_types,
                          size_t (&string_sizes)[_StorageSize],
                          size_t& pre_precision, _FirstArg first_arg,
                          _RestArgs... rest) {
    return GetArgSize(param_types[_Index], string_sizes[_Index], pre_precision,
                      first_arg) +
           GetArgSizes<_Index + 1>(param_types, string_sizes, pre_precision,
                                   rest...);
}

/**
 * @brief
 * GetArgSizes 函数的无参情况。
 */
template <size_t _Index = 0, size_t _NumArgs, size_t _StorageSize>
inline size_t GetArgSizes(const std::array<ParamType, _NumArgs>&,
                          size_t (&)[_StorageSize], size_t&) {
    return 0;
}

/**
 * @brief
 * 存储非字符串实参。
 *
 * @tparam char*、const char*、wchar_t* 和 const wchar_t* 以外的其他类型。
 * @param param_type 实参在格式串中作为的类型。
 * @param string_size 该参数在该特化中不被使用。
 * @param val 实参的值。
 * @return size_t
 */
template <typename _Tp>
typename std::enable_if<!std::is_same<_Tp, char*>::value &&
                            !std::is_same<_Tp, const char*>::value &&
                            !std::is_same<_Tp, wchar_t*>::value &&
                            !std::is_same<_Tp, const wchar_t*>::value,
                        size_t>::type
StoreArgument(char*(&dst), const ParamType& param_type,
              const size_t& string_size, _Tp val) {
    memcpy(dst, &val, sizeof(_Tp));
    dst += sizeof(_Tp);
    return sizeof(_Tp);
}

/**
 * @brief
 * 存储字符串实参。
 *
 * @tparam char* 或 const char* 或 wchar_t* 或 const wchar_t*。
 * @param param_type
 * @param string_size
 * @param val 指向字符串的指针。
 * @return size_t
 */
template <typename _Tp>
typename std::enable_if<std::is_same<_Tp, char*>::value ||
                            std::is_same<_Tp, const char*>::value ||
                            std::is_same<_Tp, wchar_t*>::value ||
                            std::is_same<_Tp, const wchar_t*>::value,
                        size_t>::type
StoreArgument(char*(&dst), const ParamType& param_type,
              const size_t& string_size, _Tp val) {
    // 显式指明参数不是字符串，将其作为指针处理。
    if (param_type <= ParamType::NON_STRING)
        return StoreArgument<const void*>(dst, param_type, string_size,
                                          static_cast<const void*>(val));

    size_t stored_bytes = 0;

    // 写入字符串所占大小。
    memcpy(dst, &string_size, sizeof(size_t));
    dst += sizeof(size_t);
    stored_bytes += sizeof(size_t);

    // 写入字符串本身。
    memcpy(dst, val, string_size);
    dst += string_size;
    stored_bytes += string_size;

    // 写入 '\0' 作为结尾。
    memcpy(dst, "\0", 1);
    dst += 1;
    stored_bytes += 1;

    return stored_bytes;
}

/**
 * @brief
 * 向目标地址存储实参。
 * 对于非字符串（char* 或 wchar_t*）类型的实参，直接存储它在内存中的原样。
 * 对于字符串类型，
 * 前 sizeof(size_t) 个字节存储字符串占用的字节数（不含 '\0'），
 * 紧接着存储字符串本身，最后一个字节存储 '\0' 作为结尾。
 *
 * @tparam _Index 遍历数组的索引，内部参数，默认为 0。
 * @tparam _NumArgs 格式串中参数的数量。
 * @tparam _StorageSize 存储字符串长度数组的大小。
 * @tparam _FirstArg 第一个实参类型。
 * @tparam _RestArgs 剩余实参类型。
 * @param param_types 格式串所需的实参类型。
 * @param first_arg 第一个实参。
 * @param rest 剩余实参。
 *
 * @return 存储的字节数。
 */
template <size_t _Index = 0, size_t _NumArgs, size_t _StorageSize,
          typename _FirstArg, typename... _RestArgs>
inline size_t StoreArguments(char*(&dst),
                             const std::array<ParamType, _NumArgs>& param_types,
                             const size_t (&string_sizes)[_StorageSize],
                             _FirstArg first_arg, _RestArgs... rest) {
    size_t stored_bytes = 0;
    stored_bytes += StoreArgument(dst, param_types[_Index],
                                  string_sizes[_Index], first_arg);
    stored_bytes +=
        StoreArguments<_Index + 1>(dst, param_types, string_sizes, rest...);
    return stored_bytes;
}

/**
 * @brief StoreArguments 的无参数特化。
 */
template <size_t _Index = 0, size_t _NumArgs, size_t _StorageSize>
inline size_t StoreArguments(char*(&dst),
                             const std::array<ParamType, _NumArgs>& param_types,
                             const size_t (&string_sizes)[_StorageSize]) {
    return 0;
}

/**
 * @brief
 * 从指定位置读出一个有符号整数类型的参数。
 *
 * @tparam _Tp 有符号整数类型。
 * @param read_pos 读位置。
 * @param nbytes 字节大小，只能是 1、2、4、8。
 * @return _Tp
 * @throw invalid_argument
 */
template <typename _Tp>
inline typename std::enable_if<(std::is_convertible<_Tp, int8_t>::value ||
                                std::is_convertible<_Tp, int16_t>::value ||
                                std::is_convertible<_Tp, int32_t>::value ||
                                std::is_convertible<_Tp, int64_t>::value) &&
                                   std::is_integral<_Tp>::value &&
                                   std::is_signed<_Tp>::value,
                               _Tp>::type
LoadArgument(const void* read_pos, size_t nbytes) {
    switch (nbytes) {
    case 1:
        return static_cast<_Tp>(*reinterpret_cast<const int8_t*>(read_pos));

    case 2:
        return static_cast<_Tp>(*reinterpret_cast<const int16_t*>(read_pos));

    case 4:
        return static_cast<_Tp>(*reinterpret_cast<const int32_t*>(read_pos));

    case 8:
        return static_cast<_Tp>(*reinterpret_cast<const ino64_t*>(read_pos));

    default:
        break;
    }
    throw std::invalid_argument(
        "Argument 'nbytes' is not one of '1, 2, 4, 8' in LoadArgument.");
}

/**
 * @brief
 * 从指定位置读出一个无符号整数类型的参数。
 *
 * @tparam _Tp 无符号整数类型。
 * @param read_pos 读位置。
 * @param nbytes 字节大小，只能是 1、2、4、8。
 * @return _Tp
 * @throw invalid_argument
 */
template <typename _Tp>
inline typename std::enable_if<(std::is_convertible<_Tp, uint8_t>::value ||
                                std::is_convertible<_Tp, uint16_t>::value ||
                                std::is_convertible<_Tp, uint32_t>::value ||
                                std::is_convertible<_Tp, uint64_t>::value) &&
                                   std::is_integral<_Tp>::value &&
                                   std::is_unsigned<_Tp>::value,
                               _Tp>::type
LoadArgument(const void* read_pos, size_t nbytes) {
    switch (nbytes) {
    case 1:
        return static_cast<_Tp>(*reinterpret_cast<const uint8_t*>(read_pos));

    case 2:
        return static_cast<_Tp>(*reinterpret_cast<const uint16_t*>(read_pos));

    case 4:
        return static_cast<_Tp>(*reinterpret_cast<const uint32_t*>(read_pos));

    case 8:
        return static_cast<_Tp>(*reinterpret_cast<const uint64_t*>(read_pos));

    default:
        break;
    }
    throw std::invalid_argument(
        "Argument 'nbytes' is not one of '1, 2, 4, 8' in LoadArgument.");
}

template <typename _Tp>
inline typename std::enable_if<(std::is_convertible<_Tp, float>::value ||
                                std::is_convertible<_Tp, double>::value ||
                                std::is_convertible<_Tp, long double>::value) &&
                                   std::is_floating_point<_Tp>::value,
                               _Tp>::type
LoadArgument(const void* read_pos, size_t nbytes) {
    switch (nbytes) {
    case sizeof(float):
        return static_cast<_Tp>(*reinterpret_cast<const float*>(read_pos));
    case sizeof(double):
        return static_cast<_Tp>(*reinterpret_cast<const double*>(read_pos));
    case sizeof(long double):
        return static_cast<_Tp>(
            *reinterpret_cast<const long double*>(read_pos));
    default:
        break;
    }
    throw std::invalid_argument(
        "Argument 'nbytes' is not one of 'sizeof(float), sizeof(double), "
        "sizeof(long double)' in LoadArgument.");
}

template <typename _Tp>
inline typename std::enable_if<std::is_same<_Tp, char*>::value ||
                                   std::is_same<_Tp, const char*>::value ||
                                   std::is_same<_Tp, wchar_t*>::value ||
                                   std::is_same<_Tp, const wchar_t*>::value ||
                                   std::is_same<_Tp, void*>::value ||
                                   std::is_same<_Tp, const void*>::value,
                               _Tp>::type
LoadArgument(const void* read_pos, size_t nbytes) {
    return reinterpret_cast<_Tp>(read_pos);
}

}  // namespace log_info
}  // namespace olog

#endif