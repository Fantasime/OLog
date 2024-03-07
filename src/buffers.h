#ifndef OLOG_BUFFERS_H
#define OLOG_BUFFERS_H

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <memory>
#include <system_error>

namespace olog {
namespace buffers {

/**
 * @brief
 * 支持单生产者单消费者的无锁循环队列。
 * 用于在工作线程和日志线程之间传递日志的动态信息。
 * 每个工作线程都有自己的一个缓冲区（被 thread_local 修饰）。
 */
class StagingBuffer {
  public:
    /**
     * @brief
     * StagingBuffer 并不由它所属的线程释放，而是被日志线程进行管理。
     * 那么就需要使日志线程“知道”工作线程已经结束，要释放对应的 StagingBuffer。
     * DestructedGuard 对 StagingBuffer 的 should_be_destruct_ 属性进行接管。
     * 当 DesturctedGuard 的存储期结束时，更新绑定的 StagingBuffer 的
     * should_be_destruct_ 属性。
     */
    class DestructGuard {
      public:
        DestructGuard();

        ~DestructGuard();

        DestructGuard(const DestructGuard&) = delete;

        DestructGuard(DestructGuard&&) = delete;

      private:
        /**
         * @brief
         * 绑定被看管的 StagingBuffer。
         * 该方法由被绑定的 StagingBuffer 调用，传递自身。
         *
         * @param bytes_pipe 指向被绑定的 StagingBuffer 的指针。
         */
        inline void bind(StagingBuffer* staging_buffer) {
            staging_buffer_ = staging_buffer;
        }

        // 指向被绑定的 StagingBuffer 的指针。
        StagingBuffer* staging_buffer_;

        friend StagingBuffer;
    };

  public:
    explicit StagingBuffer(uint32_t buffer_id, size_t capacity,
                           StagingBuffer::DestructGuard& destruct_guard)
        : producer_pos_(nullptr),
          end_of_data_(nullptr),
          consumer_pos_(nullptr),
          buffer_id_(buffer_id),
          should_be_destructed_(false),
          capacity_(capacity),
          available_bytes_(capacity),
          storage(nullptr) {
        destruct_guard.bind(this);
        storage = std::make_unique<char[]>(capacity_);
        if (storage == nullptr)
            throw std::system_error(
                errno, std::generic_category(),
                "StagingBuffer: Can't allocate space for StagingBuffer.");
        producer_pos_ = storage.get();
        end_of_data_ = storage.get() + capacity_;
        consumer_pos_ = storage.get();
    }

    ~StagingBuffer() {}

    StagingBuffer(const StagingBuffer&) = delete;

    StagingBuffer(StagingBuffer&&) = delete;

    /**
     * @brief
     * 为生产者分配指定数量的空间。
     * 当可分配空间不足时，该方法会使生产者线程自旋等待消费者腾出空间。
     *
     * @param num_bytes 请求的字节数。
     * @param blocking
     * @return char* 指向写入位置的指针。
     */
    inline char* reserveProducerSpace(size_t num_bytes, bool blocking = true) {
        // 该断言失败会导致死循环。
        assert(num_bytes < capacity_ || !blocking);

        // 如果剩余空间足够则直接分配。
        if (num_bytes < available_bytes_)
            return producer_pos_;

        // 剩余空间不足则等待消费者以获取空间。
        return reserveProducerSpaceInternal(num_bytes, blocking);
    }

    /**
     * @brief
     * 完成为生产者的分配，更新指向写入位置的指针。
     * 该方法在生产者写入完毕后调用。
     *
     * @param num_bytes 已写入的字节数。
     */
    inline void finishReservation(size_t num_bytes) {
        assert(num_bytes < available_bytes_);
        assert(producer_pos_ + num_bytes < storage.get() + capacity_);

        available_bytes_ -= num_bytes;
        producer_pos_ += num_bytes;
    }

    /**
     * @brief
     * 从缓冲区中获取数据。
     *
     * @param available_bytes 可读的字节数。
     * @return char* 指向读位置的指针。
     */
    char* peek(size_t& available_bytes);

    inline char* peek(size_t* available_bytes) {
        return peek(*available_bytes);
    }

    /**
     * @brief
     * 消费指定的字节数，更新指向读位置的指针。
     *
     * @param num_bytes 已读出的字节数。
     */
    inline void consume(size_t num_bytes) {
        assert(consumer_pos_ + num_bytes < storage.get() + capacity_);

        // fence
        consumer_pos_ += num_bytes;
    }

    inline uint32_t getId() const { return buffer_id_; }

    inline bool shouldBeDestructed() const {
        return should_be_destructed_ && consumer_pos_ == producer_pos_;
    }

    inline size_t getCapacity() const { return capacity_; }

  private:
    /**
     * @brief
     *
     * @param num_bytes 请求的字节数。
     * @param blocking 为 true 时自旋直到获得请求字节数。
     * @return char* 非阻塞且未获得请求字节数时返回 nullptr；否则返回写入位置。
     */
    char* reserveProducerSpaceInternal(size_t num_bytes, bool blocking = true);

  private:
    // 指向生产者写入位置的指针。
    // 该属性只允许被生产者更新。消费者会只读该属性，用来更新可读字节数。
    char* volatile producer_pos_;

    // 在 producer_pos_ 指回缓冲区起始位置时，该属性指向已写入数据的尾部。
    // 该属性只允许被生产者更新，用于指示消费者可读的结尾。
    char* volatile end_of_data_;

    // 指向消费者读出位置的指针。
    // 该属性只允许被消费者更新。生产者会只读该属性，用来更新可用字节数;
    char* volatile consumer_pos_;

    // 为每个对象分配的 id。
    // 当每个线程各拥有一个 StagingBuffer 对象时，该属性也是对线程的一个标记。
    uint32_t buffer_id_;

    // 指示是否可以析构该对象。
    bool should_be_destructed_;

    // 缓冲区的容量。
    size_t capacity_;

    // 可用的字节数。
    // 该属性只允许被生产者更新。
    size_t available_bytes_;

    // 指向缓冲区的指针。
    std::unique_ptr<char[]> storage;
};

}  // namespace buffers
}  // namespace olog

#endif