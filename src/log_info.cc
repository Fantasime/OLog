#include "log_info.h"

#include <ctime>

namespace olog {
namespace log_info {

bool operator==(const FormatFragment& f1, const FormatFragment& f2) {
    return f1.conversion_type_ == f2.conversion_type_ &&
           f1.specifier_length_ == f2.specifier_length_ &&
           f1.format_pos_ == f2.format_pos_ &&
           f1.storage_pos_ == f2.storage_pos_;
}

LogAssembler::LogAssembler()
    : write_pos_(nullptr),
      buffer_size_(),
      writed_count_(0),
      conversion_index_(0),
      parameter_index_(0),
      static_log_info_(nullptr),
      dynamic_log_info_(nullptr),
      timestamp_str_(),
      filename_and_linenum_(),
      producer_id_("[0]: "),
      end_of_log_("\r\n"),
      is_full_(false) {}

const StaticLogInfo* LogAssembler::loadStaticInfo(
    const StaticLogInfo* static_info) {
    const StaticLogInfo* pre = static_log_info_;
    static_log_info_ = static_info;

    if (static_log_info_ != nullptr) {
        filename_and_linenum_.clear();
        filename_and_linenum_.append(std::string_view(static_info->filename_));
        filename_and_linenum_.push_back(':');
        filename_and_linenum_.append(
            std::to_string(static_log_info_->line_number_));
        filename_and_linenum_.push_back(' ');

#ifdef OLOG_ENABLE_LOG_INFO_DEBUG_PRINTTING
        printf(
            "LogAssembler loaded static log info:\n"
            "  filename and linenum: %s\n",
            filename_and_linenum_.c_str());
#endif
    }

    resetIndices();
    resetFlags();
    return pre;
}

const DynamicLogInfo* LogAssembler::loadDynamicInfo(
    const DynamicLogInfo* dynamic_info) {
    const DynamicLogInfo* pre = dynamic_log_info_;
    dynamic_log_info_ = dynamic_info;

    if (dynamic_log_info_ != nullptr) {
        time_t timestamp_seconds = dynamic_log_info_->ms_timestamp_ / 1000;
        int64_t timestamp_milisecond = dynamic_log_info_->ms_timestamp_ % 1000;
        size_t index =
            strftime(timestamp_str_.data(), timestamp_str_.size(),
                     "%Y-%m-%d %H:%M:%S.", localtime(&timestamp_seconds));
        timestamp_str_[index++] = '0' + (timestamp_milisecond / 100);
        timestamp_str_[index++] = '0' + ((timestamp_milisecond % 100) / 10);
        timestamp_str_[index++] = '0' + (timestamp_milisecond % 10);
        timestamp_str_[index++] = ' ';

#ifdef OLOG_ENABLE_LOG_INFO_DEBUG_PRINTTING
        printf(
            "LogAssembler loaded dynamic log info:\n"
            "  timestamp: %s\n",
            timestamp_str_.data());
#endif

        args_read_pos_ = dynamic_log_info_->arg_data;
    }

    resetIndices();
    resetFlags();
    return pre;
}

void LogAssembler::setProducerId(size_t id) {
    producer_id_.clear();
    producer_id_.push_back('[');
    producer_id_.append(std::to_string(id));
    producer_id_.append("]: ");

#ifdef OLOG_ENABLE_LOG_INFO_DEBUG_PRINTTING
    printf(
        "LogAssembler changed producer id:\n"
        "  producer id: %s\n",
        producer_id_.c_str());
#endif
}

size_t LogAssembler::write() noexcept {
    if (is_full_)
        return 0;

    bytes_last_writed_ = 0;

    // 写入时间戳。
    if (!is_timestamp_writed_) {
        size_t tmp = tryToWriteAStringToBuffer(timestamp_str_.data(),
                                               timestamp_str_.size() - 1);
        if (tmp == 0)
            return bytes_last_writed_;
        finishWriting(tmp);

        is_timestamp_writed_ = true;
    }

    // 写入文件名和行号。
    if (!is_filename_and_linenum_writed_) {
        size_t tmp = tryToWriteAStringToBuffer(filename_and_linenum_.c_str(),
                                               filename_and_linenum_.size());
        if (tmp == 0)
            return bytes_last_writed_;
        finishWriting(tmp);

        is_filename_and_linenum_writed_ = true;
    }

    // 写入日志等级。
    if (!is_severity_writed_) {
        static const char* severity_str[] = {"[<none>]", "[ERROR]", "[WARNING]",
                                             "[INFO]", "[DEBUG]"};
        static const size_t severity_str_len[] = {
            std::size("[<none>]") - 1, std::size("[ERROR]") - 1,
            std::size("[WARNING]") - 1, std::size("[INFO]") - 1,
            std::size("[DEBUG]") - 1};

        size_t index = static_cast<size_t>(static_log_info_->log_level_);
        size_t tmp = tryToWriteAStringToBuffer(severity_str[index],
                                               severity_str_len[index]);
        if (tmp == 0)
            return bytes_last_writed_;
        finishWriting(tmp);

        is_severity_writed_ = true;
    }

    // 写入生产者编号。
    if (!is_producer_id_writed_) {
        size_t tmp = tryToWriteAStringToBuffer(producer_id_.c_str(),
                                               producer_id_.size());
        if (tmp == 0)
            return bytes_last_writed_;
        finishWriting(tmp);

        is_producer_id_writed_ = true;
    }

    // 写入日志主体。
    while (format_index_ < static_log_info_->format_len_) {
        if (conversion_index_ < static_log_info_->num_conversions_) {
            const FormatFragment* fragment =
                &static_log_info_->format_fragments_[conversion_index_];

            // 当前位置不指向格式描述符，写入格式串片段。
            if (format_index_ < fragment->format_pos_) {
                size_t len = fragment->format_pos_ - format_index_;
                size_t tmp = tryToWriteAStringToBuffer(
                    static_log_info_->format_str_ + format_index_, len);
                if (tmp == 0)
                    return bytes_last_writed_;
                finishWriting(tmp);
                format_index_ += tmp;
            }

            // 输出实参。
            else {
                int width = -1, precision = -1;

                // 用于写入失败时恢复处理位置。
                size_t original_conversion_index = conversion_index_,
                       original_parameter_index = parameter_index_;
                const char* original_read_pos = args_read_pos_;

                // 获取动态宽度。
                if (static_log_info_->param_types_[parameter_index_] ==
                    ParamType::DYNAMIC_WIDTH) {
                    width = LoadArgument<int>(
                        args_read_pos_,
                        static_log_info_->param_sizes_[parameter_index_]);

#ifdef OLOG_ENABLE_LOG_INFO_DEBUG_PRINTTING
                    printf(
                        "LogAssembler: parsed dynamic width: \n"
                        "  bytes: %ld, value: %d\n",
                        static_log_info_->param_sizes_[parameter_index_],
                        width);
#endif

                    args_read_pos_ +=
                        static_log_info_->param_sizes_[parameter_index_];
                    parameter_index_++;
                }

                // 获取动态精度。
                if (static_log_info_->param_types_[parameter_index_] ==
                    ParamType::DYNAMIC_PRECISION) {
                    precision = LoadArgument<int>(
                        args_read_pos_,
                        static_log_info_->param_sizes_[parameter_index_]);

#ifdef OLOG_ENABLE_LOG_INFO_DEBUG_PRINTTING
                    printf(
                        "LogAssember: parsed dynamic precision: \n"
                        "  bytes: %ld, value: %d\n",
                        static_log_info_->param_sizes_[parameter_index_],
                        precision);
#endif

                    args_read_pos_ +=
                        static_log_info_->param_sizes_[parameter_index_];
                    parameter_index_++;
                }

                /**
                 * TODO: write args.
                 */
                const char* conversion_fmt =
                    static_log_info_->conversion_storage_ +
                    fragment->storage_pos_;
                size_t arg_size =
                    static_log_info_->param_sizes_[parameter_index_];

                size_t tmp = 0;
                switch (fragment->conversion_type_) {
                case ConversionType::unsigned_char_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<unsigned char>(args_read_pos_, arg_size));
                    break;
                case ConversionType::unsigned_short_int_t:
                    tmp =
                        tryToWriteArgToBuffer(conversion_fmt, width, precision,
                                              LoadArgument<unsigned short int>(
                                                  args_read_pos_, arg_size));
                    break;
                case ConversionType::unsigned_int_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<unsigned int>(args_read_pos_, arg_size));
                    break;
                case ConversionType::unsigned_long_int_t:
                    tmp =
                        tryToWriteArgToBuffer(conversion_fmt, width, precision,
                                              LoadArgument<unsigned long int>(
                                                  args_read_pos_, arg_size));
                    break;
                case ConversionType::unsigned_long_long_int_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<unsigned long long int>(args_read_pos_,
                                                             arg_size));
                    break;
                case ConversionType::uintmax_t_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<uintmax_t>(args_read_pos_, arg_size));
                    break;
                case ConversionType::size_t_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<size_t>(args_read_pos_, arg_size));
                    break;
                case ConversionType::wint_t_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<wint_t>(args_read_pos_, arg_size));
                    break;
                case ConversionType::signed_char_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<signed char>(args_read_pos_, arg_size));
                    break;
                case ConversionType::short_int_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<short int>(args_read_pos_, arg_size));
                    break;
                case ConversionType::int_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<int>(args_read_pos_, arg_size));
                    break;
                case ConversionType::long_int_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<long int>(args_read_pos_, arg_size));
                    break;
                case ConversionType::long_long_int_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<long long int>(args_read_pos_, arg_size));
                    break;
                case ConversionType::intmax_t_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<intmax_t>(args_read_pos_, arg_size));
                    break;
                case ConversionType::ptrdiff_t_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<ptrdiff_t>(args_read_pos_, arg_size));
                    break;
                case ConversionType::double_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<double>(args_read_pos_, arg_size));
                    break;
                case ConversionType::long_double_t:
                    tmp = tryToWriteArgToBuffer(
                        conversion_fmt, width, precision,
                        LoadArgument<long double>(args_read_pos_, arg_size));
                    break;
                case ConversionType::const_void_ptr_t:
                    tmp = tryToWriteArgToBuffer<const void*>(
                        conversion_fmt, width, precision, args_read_pos_);
                    break;
                case ConversionType::const_char_ptr_t:
                    arg_size =
                        LoadArgument<size_t>(args_read_pos_, sizeof(size_t));
                    args_read_pos_ += sizeof(size_t);
                    tmp = tryToWriteArgToBuffer<const char*>(
                        conversion_fmt, width, precision, args_read_pos_);
                    args_read_pos_ += arg_size + 1;
                    break;
                case ConversionType::const_wchar_t_ptr_t:
                    arg_size =
                        LoadArgument<size_t>(args_read_pos_, sizeof(size_t));
                    args_read_pos_ += sizeof(size_t);
                    tmp = tryToWriteArgToBuffer<const wchar_t*>(
                        conversion_fmt, width, precision,
                        LoadArgument<const wchar_t*>(args_read_pos_, 0));
                    args_read_pos_ += arg_size + 1;
                    break;
                default:
                    break;
                }

                if (tmp == 0 && arg_size > 0) {
                    conversion_index_ = original_conversion_index;
                    parameter_index_ = original_parameter_index;
                    args_read_pos_ = original_read_pos;
                    return bytes_last_writed_;
                }

                finishWriting(tmp);
                args_read_pos_ +=
                    static_log_info_->param_sizes_[parameter_index_];
                ++conversion_index_;
                ++parameter_index_;
                format_index_ += fragment->specifier_length_;
            }
        }

        // 没有格式描述符，直接写入格式串片段。
        else {
            size_t len = static_log_info_->format_len_ - format_index_;
            size_t tmp = tryToWriteAStringToBuffer(
                static_log_info_->format_str_ + format_index_, len);
            if (tmp == 0)
                return bytes_last_writed_;
            finishWriting(tmp);
            format_index_ += tmp;
        }
    }

    // 换行。
    if (!is_end_of_log_writed_) {
        size_t tmp =
            tryToWriteAStringToBuffer(end_of_log_.data(), end_of_log_.size());
        if (tmp == 0)
            return bytes_last_writed_;
        finishWriting(tmp);
        is_end_of_log_writed_ = true;
    }

    return bytes_last_writed_;
}

}  // namespace log_info
}  // namespace olog
