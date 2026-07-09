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
                               std::vector<float>& rear_right_slip,
                               std::vector<float>& rear_left_rps_ratio,
                               std::vector<float>& rear_right_rps_ratio) const {
    std::lock_guard<std::mutex> lock(mutex_);

    timestamps.resize(count_);
    throttle.resize(count_);
    rear_left_pwm.resize(count_);
    rear_right_pwm.resize(count_);
    rear_left_slip.resize(count_);
    rear_right_slip.resize(count_);
    rear_left_rps_ratio.resize(count_);
    rear_right_rps_ratio.resize(count_);

    // Oldest sample is at (head_ - count_ + capacity_) % capacity_
    const size_t oldest = (head_ + capacity_ - count_) % capacity_;
    for (size_t i = 0; i < count_; ++i) {
        const size_t idx = (oldest + i) % capacity_;
        const auto& s = buf_[idx];
        timestamps[i]          = static_cast<float>(s.timestampMs);
        throttle[i]            = s.throttle;
        rear_left_pwm[i]       = s.rear_left_pwm;
        rear_right_pwm[i]      = s.rear_right_pwm;
        rear_left_slip[i]      = s.rear_left_slip;
        rear_right_slip[i]     = s.rear_right_slip;
        rear_left_rps_ratio[i] = s.rear_left_rps_ratio;
        rear_right_rps_ratio[i]= s.rear_right_rps_ratio;
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
    // Sync strategy: look for START_OF_FRAME_MARKER, then a known message
    // type, then the fixed-size payload. Any mismatch drops us back to
    // searching for the next SOF byte, which resyncs after corrupted or
    // lost bytes without relying on idle gaps between frames.

    // Read timeouts are configured once when the port is opened
    // (BluetoothSerial::connect); framing no longer needs its own.

    enum class State { WaitSof, WaitType, AccumPayload };
    State state = State::WaitSof;

    uint8_t stagingBuf[MESSAGE_OUT_SIZE];
    size_t  stagingLen = 0;

    // Timestamp of when the SOF byte of the current frame arrived.
    double frameStartMs = 0.0;

    uint8_t readBuf[64];

    while (!stopRequested_.load()) {
        DWORD bytesRead = 0;
        if (!ReadFile(h, readBuf, sizeof(readBuf), &bytesRead, nullptr) || bytesRead == 0) {
            continue;
        }

        for (DWORD i = 0; i < bytesRead; ++i) {
            const uint8_t byte = readBuf[i];

            if (state == State::WaitSof) {
                if (byte == START_OF_FRAME_MARKER) {
                    stagingLen               = 0;
                    stagingBuf[stagingLen++] = byte;
                    state                    = State::WaitType;

                    const auto now = std::chrono::steady_clock::now();
                    frameStartMs   = std::chrono::duration<double, std::milli>(now - epoch_).count();
                }
                // Not a SOF byte: discard and keep scanning.
                continue;
            }

            if (state == State::WaitType) {
                if (byte == static_cast<uint8_t>(MSG_OUT_TYPE_LOG)) {
                    stagingBuf[stagingLen++] = byte;
                    state                    = State::AccumPayload;
                } else {
                    // Not a recognised type — this byte can't start a frame
                    // either unless it's itself a SOF, so re-test it there.
                    state = State::WaitSof;
                    --i;
                }
                continue;
            }

            // State::AccumPayload
            stagingBuf[stagingLen++] = byte;
            if (stagingLen == MESSAGE_OUT_SIZE) {
                MessageOut_t msg{};
                std::memcpy(&msg, stagingBuf, MESSAGE_OUT_SIZE);

                LogSample sample{};
                sample.timestampMs    = frameStartMs;
                sample.throttle       = msg.payload.log.throttle       / 255.0f;
                sample.rear_left_pwm  = msg.payload.log.rear_left_pwm  / 255.0f;
                sample.rear_right_pwm = msg.payload.log.rear_right_pwm / 255.0f;
                sample.rear_left_slip       = msg.payload.log.rear_left_slip       / 255.0f;
                sample.rear_right_slip      = msg.payload.log.rear_right_slip      / 255.0f;
                sample.rear_left_rps_ratio  = msg.payload.log.rear_left_rps_ratio  * (2.0f / 255.0f);
                sample.rear_right_rps_ratio = msg.payload.log.rear_right_rps_ratio * (2.0f / 255.0f);

                buffer_.push(sample);

                state = State::WaitSof;
            }
        }
    }
}

}  // namespace bluestick
