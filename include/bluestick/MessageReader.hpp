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
    float front_left_rpm;
    float front_right_rpm;
    float rear_left_rpm;
    float rear_right_rpm;
    float rear_left_target_rpm;
    float rear_right_target_rpm;
    float rear_left_pwm;
    float rear_right_pwm;
    float real_rpm;         // max(front_left_rpm, front_right_rpm), normalised 0.0-1.0
    float rear_left_slip;   // rear_left_rpm / real_rpm — unitless, can exceed 1.0
    float rear_right_slip;  // rear_right_rpm / real_rpm — unitless, can exceed 1.0
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
                    std::vector<float>& front_left_rpm,
                    std::vector<float>& front_right_rpm,
                    std::vector<float>& rear_left_rpm,
                    std::vector<float>& rear_right_rpm,
                    std::vector<float>& rear_left_target_rpm,
                    std::vector<float>& rear_right_target_rpm,
                    std::vector<float>& rear_left_pwm,
                    std::vector<float>& rear_right_pwm,
                    std::vector<float>& real_rpm,
                    std::vector<float>& rear_left_slip,
                    std::vector<float>& rear_right_slip) const;

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
