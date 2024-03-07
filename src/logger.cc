#include "logger.h"

#include <liburing.h>

#include <cstdlib>
#include <ios>
#include <mutex>
#include <thread>

#include "buffers.h"
#include "fcntl.h"
#include "log_info.h"
#include "unistd.h"

namespace olog {
namespace logger {

// 初始化 Logger 的静态变量。
thread_local buffers::StagingBuffer* Logger::staging_buffer_ = nullptr;
thread_local buffers::StagingBuffer::DestructGuard
    Logger::staging_buffer_destruct_guard_ = {};

Logger& Logger::GetInstance() {
    static Logger logger_instance{};
    return logger_instance;
}

Logger::Logger()
    : current_log_level_(log_info::LogLevel::INFO),
      output_fd_(STDOUT_FILENO),
      ring(),
      num_sqes_(0) {
    buffer_for_log_ = std::make_unique<char[]>(config::DOUBLE_BUFFER_SIZE);
    buffer_for_io_ = std::make_unique<char[]>(config::DOUBLE_BUFFER_SIZE);

    int ret = io_uring_queue_init(config::IO_URING_ENTRIES, &ring,
                                  config::IO_URING_INIT_FLAGS);

    if (ret < 0) {
        fprintf(stderr, "OLog can't init io_uring queue: %s", strerror(-ret));
        exit(EXIT_FAILURE);
    }

    consumer_thread_ = std::thread(&Logger::consumerThreadMain, this);
}

Logger::~Logger() {
    consumer_should_exit_ = true;
    if (consumer_thread_.joinable())
        consumer_thread_.join();
#ifdef OLOG_ENABLE_LOGGER_DEBUG_PRINTTING
    printf("Logger: remaining number of producer buffers is: %ld\n",
           producer_buffers_.size());
#endif
    if (output_fd_ > 0 && output_fd_ != STDOUT_FILENO)
        close(output_fd_);
    io_uring_queue_exit(&ring);
}

void Logger::registerLogInfoInternal(int& log_id,
                                     log_info::StaticLogInfo static_log_info) {
    std::lock_guard<std::mutex> lock(registered_info_mtx_);

    if (log_id != log_info::UNREGISTERED_LOG_ID)
        return;

    // 为其分配 id。
    log_id = static_cast<int>(registered_info_.size());
    registered_info_.emplace_back(std::move(static_log_info));

#ifdef OLOG_ENABLE_LOGGER_DEBUG_PRINTTING
    printf("Logger assigned id: %d for: \n", log_id);
    printf("  %s:%d \"%s\"\n", static_log_info.filename_,
           static_log_info.line_number_, static_log_info.format_str_);
#endif
}

void Logger::setLogLevelInternal(log_info::LogLevel level) {
    if (level >= log_info::LogLevel::NUMBER_OF_LOG_LEVELS)
        level = static_cast<log_info::LogLevel>(
            static_cast<uint8_t>(log_info::LogLevel::NUMBER_OF_LOG_LEVELS) - 1);
    current_log_level_ = level;
}

void Logger::setLogFileInternal(const char* filename) {
    // 检查文件是否可以进行读写。
    if (access(filename, F_OK) == 0 && access(filename, R_OK | W_OK) != 0) {
        std::string err_msg = "Unable to read/write file: ";
        err_msg.append(filename);
        throw std::ios_base::failure(err_msg);
    }

    // 尝试打开文件。
    int new_fd = open(filename, config::LOG_FILE_FLAGS, 0666);
    if (new_fd < 0) {
        std::string err_msg = "Can't open file: ";
        err_msg.append(filename);
        err_msg.append(": ");
        err_msg.append(strerror(errno));
        throw std::ios_base::failure(err_msg);
    }

    if (output_fd_ > 0)
        close(output_fd_);
    output_fd_ = new_fd;
}

void Logger::updateShadowRegisteredInfo() {
    std::lock_guard<std::mutex> lock(registered_info_mtx_);
    for (auto i = shadow_registered_info_.size(); i < registered_info_.size();
         ++i) {
        shadow_registered_info_.push_back(registered_info_[i]);
    }
}

int Logger::waitForIoUring() {
    if (num_sqes_ == 0)
        return 0;
    io_uring_cqe* cqe = nullptr;
    int ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret == 0)
        io_uring_cqe_seen(&ring, cqe);
    return ret;
}

int Logger::submitLog(size_t nbytes) {
    if (nbytes == 0)
        return 0;
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_write(sqe, output_fd_, buffer_for_io_.get(), nbytes, 0);
    int ret = io_uring_submit(&ring);
    if (ret == 0)
        num_sqes_++;
    return ret;
}

void Logger::consumerThreadMain() {
    log_info::LogAssembler log_assembler;
    log_assembler.setBuffer(buffer_for_log_.get(), config::DOUBLE_BUFFER_SIZE);

    /* 指示等待 io_uring 任务完成的标志。 */
    bool has_outstanding_operation = false;

    /* 即使主线程指示日志线程应当退出，但当缓冲区中还存在数据时，
     * 日志线程应该在这些数据被处理后再退出。
     */
    while (!consumer_should_exit_ || has_outstanding_operation) {
        /* 轮询各生产者的缓冲区，读取日志动态信息。*/
        {
            std::unique_lock<std::mutex> producer_buffers_lock(
                producer_buffers_mtx_);

            for (size_t consuming_buffer_idx = 0;
                 consuming_buffer_idx < producer_buffers_.size();
                 ++consuming_buffer_idx) {
                /* 处理 consuming_buffer_idx 指向的缓冲区。*/
                buffers::StagingBuffer* consuming_buffer =
                    producer_buffers_[consuming_buffer_idx];

                size_t peek_bytes = 0;
                char* read_pos = consuming_buffer->peek(&peek_bytes);
                if (peek_bytes > 0) {
                    /* 有日志可写时使用 log_assembler
                     * 将日志恢复并写入缓冲区。*/
                    producer_buffers_lock.unlock();

                    size_t bytes_consumed = 0;
                    while (bytes_consumed < peek_bytes) {
                        log_info::DynamicLogInfo* dynamic_log_info =
                            reinterpret_cast<log_info::DynamicLogInfo*>(
                                read_pos);

                        if (dynamic_log_info->log_id_ >=
                            shadow_registered_info_.size()) {
                            // 动态信息对应的静态信息并未被日志线程复制，手动更新副本。
                            updateShadowRegisteredInfo();
                        }

                        log_info::StaticLogInfo* static_log_info =
                            &shadow_registered_info_[dynamic_log_info->log_id_];

                        // 装载对应的静态信息、动态信息和生产者编号。
                        log_assembler.loadLogInfo(static_log_info,
                                                  dynamic_log_info,
                                                  consuming_buffer_idx);

                        // 将日志恢复并写入缓冲区。
                        while (log_assembler.hasRemainingData()) {
                            size_t bytes_writed = log_assembler.write();
                            if (log_assembler.isBufferFull()) {
                                swapDoubleBuffer(
                                    log_assembler.getWritedBytes());
                                log_assembler.setBuffer(
                                    buffer_for_log_.get(),
                                    config::DOUBLE_BUFFER_SIZE);
                            }
                        }

                        bytes_consumed += dynamic_log_info->info_size_;
                        read_pos += dynamic_log_info->info_size_;
                        consuming_buffer->consume(dynamic_log_info->info_size_);
                    }

                    producer_buffers_lock.lock();
                } else {
                    /* 没有说明该缓冲区可能已经被生产者弃用（生产者线程退出），
                     * 检查它能否被释放。
                     */
                    if (consuming_buffer->shouldBeDestructed()) {
                        delete consuming_buffer;

                        producer_buffers_.erase(producer_buffers_.begin() +
                                                consuming_buffer_idx);
                        if (!producer_buffers_.empty())
                            --consuming_buffer_idx;
                    }
                }
            }
        }

        has_outstanding_operation = false;
        if (log_assembler.getWritedBytes() == 0) {
            /* 暂时没有日志可写。 */
        } else {
            /* 更换缓冲区。 */
            swapDoubleBuffer(log_assembler.getWritedBytes());
            log_assembler.setBuffer(buffer_for_log_.get(),
                                    config::DOUBLE_BUFFER_SIZE);
            has_outstanding_operation = true;
        }
    }
}

}  // namespace logger
}  // namespace olog
