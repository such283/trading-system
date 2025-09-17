//
// Created by Supradeep Chitumalla on 16/09/25.
//

#ifndef BUFFER_H
#define BUFFER_H
#include <atomic>
#include <optional>

namespace deribit {
    template<typename T>
    class Buffer {
    private:
        std::vector<T>buffer_;
        size_t capacity_;
        alignas(64) std::atomic<size_t> head_;
        alignas(64) std::atomic<size_t> tail_;

    public:
        explicit Buffer(size_t capacity) : capacity_(capacity),buffer_(capacity),head_(0), tail_(0) {

        }
        bool push(const T& item) {
            size_t head = head_.load(std::memory_order_relaxed);
            size_t next_head = (head + 1) % capacity_;
            if (next_head == tail_.load(std::memory_order_acquire)) {
                return false;
            }
            buffer_[head] = item;
            head_.store(next_head, std::memory_order_release);
            return true;
        }

        std::optional<T> pop() {
            while (true) {
                size_t tail = tail_.load(std::memory_order_acquire);
                size_t head = head_.load(std::memory_order_acquire);

                if (tail == head) {
                    return std::nullopt;
                }
                T value = buffer_[tail];
                if (tail_.compare_exchange_weak(tail, (tail + 1) % capacity_,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
                    return value;
                                               }
            }
        }

    };
}

#endif //BUFFER_H
