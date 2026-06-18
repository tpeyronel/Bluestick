#include "bluestick/MessageReader.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace bluestick {

// ---- LogRingBuffer ----

LogRingBuffer::LogRingBuffer(size_t capacity)
    : capacity_(capacity), buf_(capacity) {}

void LogRingBuffer::push(const LogSample& sample) {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_[head_] = sample;
    head_ = (head_ + 1) % capacity_;
    if (count_ < capacity_) {
        ++count_;
    }
}

void LogRingBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    count_ = 0;
    head_  = 0;
}

size_t LogRingBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

size_t LogRingBuffer::snapshot(std::vector<float>& timestamps,
                               std::vector<float>& throttle,
                               std::vector<float>& rear_left_pwm,
                               std::vector<float>& rear_right_pwm,
                               std::vector<float>& rear_left_slip,
                               std::vector<float>& rear_right_slip) const {
    std::lock_guard<std::mutex> lock(mutex_);

    timestamps.resize(count_);
    throttle.resize(count_);
    rear_left_pwm.resize(count_);
    rear_right_pwm.resize(count_);
    rear_left_slip.resize(count_);
    rear_right_slip.resize(count_);

    // Oldest sample is at (head_ - count_ + capacity_) % capacity_
    const size_t oldest = (head_ + capacity_ - count_) % capacity_;
    for (size_t i = 0; i < count_; ++i) {
        const size_t idx = (oldest + i) % capacity_;
        const auto& s = buf_[idx];
        timestamps[i]     = static_cast<float>(s.timestampMs);
        throttle[i]       = s.throttle;
        rear_left_pwm[i]  = s.rear_left_pwm;
        rear_right_pwm[i] = s.rear_right_pwm;
        rear_left_slip[i] = s.rear_left_slip;
        rear_right_slip[i]= s.rear_right_slip;
    }

    return count_;
}

// ---- MessageReader ----

MessageReader::MessageReader() = default;

MessageReader::~MessageReader() {
    stop();
}

void MessageReader::start(HANDLE serialHandle, double /*sampleRateHz*/) {
    stop();

    epoch_ = std::chrono::steady_clock::now();
    buffer_.clear();
    stopRequested_.store(false);
    running_.store(true);
    thread_ = std::thread(&MessageReader::readerLoop, this, serialHandle);
}

void MessageReader::stop() {
    stopRequested_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    running_.store(false);
}

void MessageReader::readerLoop(HANDLE h) {
    // We accumulate raw bytes into a small staging buffer.
    // An idle gap (no bytes for kIdleGapMs) resets accumulation — this is
    // the sync strategy. A complete MessageOutLog is MESSAGE_OUT_LOG_SIZE bytes.
    constexpr DWORD kIdleGapMs = 5;

    // Update the COM port read timeout to match our idle gap heuristic.
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout         = kIdleGapMs;
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.WriteTotalTimeoutConstant   = 100;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(h, &timeouts);

    uint8_t stagingBuf[MESSAGE_OUT_LOG_SIZE];
    size_t  stagingLen = 0;
    bool    inFrame    = false;

    // Timestamp of when the first byte of the current frame arrived.
    double frameStartMs = 0.0;

    uint8_t readBuf[64];

    while (!stopRequested_.load()) {
        DWORD bytesRead = 0;
        if (!ReadFile(h, readBuf, sizeof(readBuf), &bytesRead, nullptr) || bytesRead == 0) {
            // Timeout (ReadIntervalTimeout expired) → treat as idle gap → reset frame.
            if (stagingLen > 0) {
                stagingLen = 0;
                inFrame    = false;
            }
            continue;
        }

        for (DWORD i = 0; i < bytesRead; ++i) {
            const uint8_t byte = readBuf[i];

            if (!inFrame) {
                // Wait for a valid type byte.
                if (byte == static_cast<uint8_t>(MSG_OUT_TYPE_LOG)) {
                    inFrame    = true;
                    stagingLen = 0;

                    const auto now = std::chrono::steady_clock::now();
                    frameStartMs   = std::chrono::duration<double, std::milli>(now - epoch_).count();

                    stagingBuf[stagingLen++] = byte;
                }
                // Unknown type byte: discard and wait.
            } else {
                stagingBuf[stagingLen++] = byte;

                if (stagingLen == MESSAGE_OUT_LOG_SIZE) {
                    // Full frame — parse it.
                    MessageOutLog msg{};
                    std::memcpy(&msg, stagingBuf, MESSAGE_OUT_LOG_SIZE);

                    LogSample sample{};
                    sample.timestampMs    = frameStartMs;
                    sample.throttle       = msg.throttle       / 255.0f;
                    sample.rear_left_pwm  = msg.rear_left_pwm  / 255.0f;
                    sample.rear_right_pwm = msg.rear_right_pwm / 255.0f;
                    sample.rear_left_slip = msg.rear_left_slip / 255.0f;
                    sample.rear_right_slip= msg.rear_right_slip/ 255.0f;

                    buffer_.push(sample);

                    stagingLen = 0;
                    inFrame    = false;
                }

                if (stagingLen > MESSAGE_OUT_LOG_SIZE) {
                    // Should never happen, but guard against it.
                    stagingLen = 0;
                    inFrame    = false;
                }
            }
        }
    }
}

}  // namespace bluestick
