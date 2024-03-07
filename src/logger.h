#ifndef OLOG_LOGGER_H
#define OLOG_LOGGER_H

#include <liburing.h>

#include <mutex>
#include <thread>
#include <vector>

#include "buffers.h"
#include "log_info.h"
#include "olog_config.h"

namespace olog {

/**
 *
 *
 */
namespace logger {

class Logger {
  public:
    /**
     * @brief
     * 获取 Logger 单例。
     *
     * @return Logger&
     */
    static Logger& GetInstance();

    /**
     * @brief
     * 向 Logger 单例注册日志。
     * Logger 单例会保存日志静态信息方便复用。
     *
     * @param static_log_info 被注册的日志静态信息。
     * @param log_id 对注册 id 的引用。
     */
    static inline void RegisterLogInfo(log_info::StaticLogInfo static_log_info,
                                       int& log_id) {
        GetInstance().registerLogInfoInternal(log_id, static_log_info);
    }

    /**
     * @brief
     * 在缓冲区中预留指定大小的字节。
     *
     * @param num_bytes 要分配的字节数。
     * @return 写入位置。
     */
    static inline char* ReserveAlloc(size_t num_bytes) {
        if (staging_buffer_ == nullptr)
            GetInstance().ensureBufferIsAllocated();
        return staging_buffer_->reserveProducerSpace(num_bytes);
    }

    /**
     * @brief
     * 完成对缓冲区的分配。
     *
     * @param num_bytes 使用了的字节数。
     */
    static inline void FinishAlloc(size_t num_bytes) {
        staging_buffer_->finishReservation(num_bytes);
    }

    /**
     * @brief
     * 设置最高的允许输出的日志等级。
     *
     * @param level 指定的日志等级。
     */
    static inline void SetLogLevel(log_info::LogLevel level) {
        GetInstance().setLogLevelInternal(level);
    }

    /**
     * @brief
     * 获取当前允许输出的最高日志等级。
     *
     * @return 允许输出的最高日志等级。
     */
    static inline log_info::LogLevel GetLogLevel() {
        return GetInstance().getLogLevelInternal();
    }

    /**
     * @brief
     * 设置日志的输出文件。
     *
     * @param filename 文件名。
     */
    static inline void SetLogFile(const char* filename) {
        GetInstance().setLogFileInternal(filename);
    }

  private:
    Logger();

    ~Logger();

    Logger(const Logger&) = delete;

    Logger(Logger&&) = delete;

    Logger& operator=(const Logger&) = delete;

    /**
     * @brief
     * 确保线程的缓冲区已被分配。
     */
    inline void ensureBufferIsAllocated() {
        if (staging_buffer_ == nullptr) {
            // 保护 next_buffer_id_，加锁。
            std::unique_lock<std::mutex> lock(producer_buffers_mtx_);
            uint32_t buffer_id = next_buffer_id_++;

            // new 一个新的 StagingBuffer 不需要加锁。
            lock.unlock();
            staging_buffer_ = new buffers::StagingBuffer{
                buffer_id, config::STORAGE_BUFFER_SIZE,
                staging_buffer_destruct_guard_};

            // 加锁保护 producer_buffers_。
            lock.lock();
            producer_buffers_.push_back(staging_buffer_);
        }
    }

    /**
     * @brief
     * RegisterLogInfo 的内部方法，
     *
     * @param log_id 对注册 id 的引用。
     * @param static_log_info 被注册的日志静态信息。
     */
    void registerLogInfoInternal(int& log_id,
                                 log_info::StaticLogInfo static_log_info);

    /**
     * @brief
     * 设置最高的允许输出的日志等级。
     *
     * @param level 允许输出的最高日志等级。
     */
    void setLogLevelInternal(log_info::LogLevel level);

    /**
     * @brief
     * 获取当前允许输出的最高日志等级。
     *
     * @return log_info::LogLevel
     */
    inline log_info::LogLevel getLogLevelInternal() const {
        return current_log_level_;
    }

    /**
     * @brief
     * 设置日志输出文件。
     *
     * @param filename 文件名。
     *
     * @throw
     */
    void setLogFileInternal(const char* filename);

    /**
     * @brief
     * 从 registered_info_ 中复制未复制的内容到 shadow_registered_info_。
     */
    void updateShadowRegisteredInfo();

    /**
     * @brief
     * 将 buffer_for_log_ 和 buffer_for_io_ 的内容交换。
     * 将日志线程使用的缓冲区和 io_uring 要写出的缓冲区交换。
     *
     * @param bytes_in_buffer_for_log buffer_for_log_ 中写入的字节数
     */
    inline void swapDoubleBuffer(size_t bytes_in_buffer_for_log) {
        int ring_ret = waitForIoUring();
        if (ring_ret < 0)
            fprintf(stderr,
                    "An error occurs when Logger is waiting for io_uring, your "
                    "log message may be incomplete: %s\n",
                    strerror(-ring_ret));
        buffer_for_io_.swap(buffer_for_log_);
        ring_ret = submitLog(bytes_in_buffer_for_log);
        if (ring_ret < 0)
            fprintf(stderr,
                    "An error occurs when Logger is writing, your log message "
                    "may be incomplete: %s\n",
                    strerror(-ring_ret));
    }

    /**
     * @brief 等待 io_uring 完成。
     *
     * @return io_uring_wait_cqe 的返回值。
     */
    int waitForIoUring();

    /**
     * @brief
     * 提交 buffer_for_io_ 到 io_uring，将内容写到 output_fd_ 对应的文件中。
     *
     * @param nbytes 缓冲区中数据的大小。
     * @return io_uring_submit 的返回值。
     */
    int submitLog(size_t nbytes);

    /**
     * @brief
     * 日志线程的主函数。
     * 后台线程从各个工作线程（生产者）的 staging_buffer_
     * 中读出日志的动态信息，并与其对应的日志静态信息进行处理，
     * 写入 Logger 中指定的文件。
     */
    void consumerThreadMain();

  private:
    // 当前允许输出的最高日志等级，比该等级高的日志会被忽略。
    log_info::LogLevel current_log_level_;

    // 日志输出文件的格式描述符。
    int output_fd_;

    // io_uring 的数据结构。
    io_uring ring;

    // 提交到 sq 上的 sqe 数量。
    unsigned int num_sqes_;

    // 对 registered_info_ 进行保护。
    std::mutex registered_info_mtx_;

    // 存储已注册的日志的静态信息。日志线程会复用这些信息。
    // 生产者会对该属性进行写操作，而日志线程（消费者）会
    // 对其进行读操作，该属性需要用锁控制的正确性。
    std::vector<log_info::StaticLogInfo> registered_info_;

    // registered_info_ 内容的副本。
    // 日志线程需要大量访问静态日志信息，加之注册期间 vector
    // 可能会发生扩容，所以这里使用副本仅供日志线程使用。
    std::vector<log_info::StaticLogInfo> shadow_registered_info_;

    // 为每个线程都分配一个单独的缓冲区，用于传输日志的动态信息。
    static thread_local buffers::StagingBuffer* staging_buffer_;

    // 通知日志线程析构相应线程的缓冲区。
    static thread_local buffers::StagingBuffer::DestructGuard
        staging_buffer_destruct_guard_;

    // 对 producer_buffers_ 和 next_buffer_id_ 进行保护。
    std::mutex producer_buffers_mtx_;

    // 存储所有生产者的 StagingBuffer。
    std::vector<buffers::StagingBuffer*> producer_buffers_;

    // 为下一个线程的 staging_buffer_ 分配的 id。
    uint32_t next_buffer_id_;

    std::unique_ptr<char[]> buffer_for_log_;

    std::unique_ptr<char[]> buffer_for_io_;

    // 写入日志的线程。
    std::thread consumer_thread_;

    // 指示日志线程应该结束工作。
    bool consumer_should_exit_;
};

}  // namespace logger
}  // namespace olog

#endif