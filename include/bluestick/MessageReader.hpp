#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include <windows.h>

#include "protocol.h"

namespace bluestick {

struct LogSample {
    double timestampMs;   // ms since reader was started (first connect)
    float throttle;
    float rear_left_pwm;
    float rear_right_pwm;
    float rear_left_slip;
    float rear_right_slip;
    float rear_left_rps_ratio;   // 0.0-2.0 (current RPS / target RPS)
    float rear_right_rps_ratio;
};

// Circular buffer of LogSamples, fixed capacity.
class LogRingBuffer {
public:
    explicit LogRingBuffer(size_t capacity);

    void push(const LogSample& sample);
    void clear();

    // Copy all samples out in chronological order (oldest first).
    // Fills xs/ys arrays for each channel; returns number of samples copied.
    size_t snapshot(std::vector<float>& timestamps,
                    std::vector<float>& throttle,
                    std::vector<float>& rear_left_pwm,
                    std::vector<float>& rear_right_pwm,
                    std::vector<float>& rear_left_slip,
                    std::vector<float>& rear_right_slip,
                    std::vector<float>& rear_left_rps_ratio,
                    std::vector<float>& rear_right_rps_ratio) const;

    size_t size() const;
    size_t capacity() const { return capacity_; }

private:
    size_t capacity_;
    size_t count_  = 0;
    size_t head_   = 0;   // next write position
    std::vector<LogSample> buf_;
    mutable std::mutex mutex_;
};

// Spawns a background thread that reads from a Windows serial HANDLE,
// assembles MessageOut_t frames, timestamps them, and appends to a ring buffer.
// Synchronization strategy: scans for START_OF_FRAME_MARKER to resync after
// corrupted or lost bytes.
class MessageReader {
public:
    MessageReader();
    ~MessageReader();

    MessageReader(const MessageReader&) = delete;
    MessageReader& operator=(const MessageReader&) = delete;

    // Start reading from the given handle. The handle must remain valid until stop().
    void start(HANDLE serialHandle, double sampleRateHz = 100.0);
    void stop();
    bool isRunning() const { return running_.load(); }

    LogRingBuffer& buffer() { return buffer_; }

private:
    void readerLoop(HANDLE h);

    static constexpr size_t kCapacity = 30 * 500;  // 30s @ up to 500 Hz

    LogRingBuffer buffer_{kCapacity};
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};

    // Epoch for timestamps (set on start()).
    std::chrono::steady_clock::time_point epoch_;
};

}  // namespace bluestick
