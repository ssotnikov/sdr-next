#pragma once
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <utility>

namespace sdr {
template <class T> class BoundedQueue {
  public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (!capacity)
            throw std::invalid_argument("Queue capacity must be positive");
    }
    bool push(T value) {
        std::lock_guard lock(mutex_);
        if (queue_.size() == capacity_)
            return false;
        queue_.push_back(std::move(value));
        return true;
    }
    bool push_for(T value, std::chrono::milliseconds timeout, std::stop_token stop,
                  std::size_t *depth_after = nullptr) {
        std::unique_lock lock(mutex_);
        if (stop.stop_requested() ||
            !space_.wait_for(lock, stop, timeout, [this] { return queue_.size() < capacity_; })) {
            if (depth_after)
                *depth_after = queue_.size();
            return false;
        }
        queue_.push_back(std::move(value));
        if (depth_after)
            *depth_after = queue_.size();
        return true;
    }
    std::optional<T> pop(std::size_t *depth_after = nullptr) {
        std::lock_guard lock(mutex_);
        if (queue_.empty())
            return {};
        T value = std::move(queue_.front());
        queue_.pop_front();
        if (depth_after)
            *depth_after = queue_.size();
        space_.notify_one();
        return value;
    }
    void clear() {
        std::lock_guard lock(mutex_);
        queue_.clear();
        space_.notify_all();
    }
    std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

  private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable_any space_;
    std::deque<T> queue_;
};
} // namespace sdr
