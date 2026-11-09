#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

class AudioTrace {
  public:
    using Clock = std::chrono::steady_clock;
    enum class Kind { iq_read, iq_enqueue, dsp, push, drop, read, prefill, underrun, clear, record, sink_state };
    struct Event {
        Kind kind;
        std::uint64_t block = 0;
        std::size_t frames = 0, actual = 0, before = 0, after = 0;
        std::int64_t time_us = 0, duration_us = 0;
    };
    static constexpr std::size_t capacity = 4096;

    AudioTrace() { pending_.reserve(capacity); }

    // Producers never wait for the collector or allocate.
    void add(Event event, Clock::time_point end = Clock::now()) {
        event.time_us = micros(end - origin_);
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (lock && pending_.size() < capacity)
            pending_.push_back(event);
        else
            lost_.fetch_add(1, std::memory_order_relaxed);
    }

    void take(std::vector<Event> &batch) {
        batch.clear();
        batch.reserve(capacity);
        std::lock_guard lock(mutex_);
        batch.swap(pending_);
    }

    std::uint64_t lost() const { return lost_.load(std::memory_order_relaxed); }
    static std::int64_t micros(Clock::duration value) {
        return std::chrono::duration_cast<std::chrono::microseconds>(value).count();
    }

  private:
    const Clock::time_point origin_ = Clock::now();
    std::mutex mutex_;
    std::vector<Event> pending_;
    std::atomic<std::uint64_t> lost_{0};
};
