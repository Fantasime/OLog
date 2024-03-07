#include "buffers.h"

namespace olog {
namespace buffers {

StagingBuffer::DestructGuard::DestructGuard()
    : staging_buffer_(nullptr) {}

StagingBuffer::DestructGuard::~DestructGuard() {
    if (staging_buffer_ != nullptr) {
        staging_buffer_->should_be_destructed_ = true;
        staging_buffer_ = nullptr;
    }
}

char* StagingBuffer::peek(size_t& available_bytes) {
    char* cached_producer_pos = producer_pos_;

    if (cached_producer_pos < consumer_pos_) {
        available_bytes = end_of_data_ - consumer_pos_;

        if (available_bytes > 0)
            return consumer_pos_;

        consumer_pos_ = storage.get();
    }

    available_bytes = cached_producer_pos - consumer_pos_;
    return consumer_pos_;
}

char* StagingBuffer::reserveProducerSpaceInternal(size_t num_bytes, bool blocking) {
    const char* end_of_storage = storage.get() + capacity_;

    while (available_bytes_ <= num_bytes) {
        char* cached_consumer_pos = consumer_pos_;

        if (cached_consumer_pos <= producer_pos_) {
            available_bytes_ = end_of_storage - producer_pos_;

            if (available_bytes_ > num_bytes)
                return producer_pos_;

            end_of_data_ = producer_pos_;

            if (cached_consumer_pos != storage.get()) {
                producer_pos_ = storage.get();
                available_bytes_ = cached_consumer_pos - producer_pos_;
            }
        } else {
            available_bytes_ = cached_consumer_pos - consumer_pos_;
        }

        if (!blocking && available_bytes_ <= num_bytes)
            return nullptr;
    }

    return producer_pos_;
}

}  // namespace buffers
}  // namespace olog
