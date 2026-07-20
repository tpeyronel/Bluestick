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
                               std::vector<float>& rear_right_slip) const {
    std::lock_guard<std::mutex> lock(mutex_);

    timestamps.resize(count_);
    throttle.resize(count_);
    front_left_rpm.resize(count_);
    front_right_rpm.resize(count_);
    rear_left_rpm.resize(count_);
    rear_right_rpm.resize(count_);
    rear_left_target_rpm.resize(count_);
    rear_right_target_rpm.resize(count_);
    rear_left_pwm.resize(count_);
    rear_right_pwm.resize(count_);
    real_rpm.resize(count_);
    rear_left_slip.resize(count_);
    rear_right_slip.resize(count_);

    // Oldest sample is at (head_ - count_ + capacity_) % capacity_
    const size_t oldest = (head_ + capacity_ - count_) % capacity_;
    for (size_t i = 0; i < count_; ++i) {
        const size_t idx = (oldest + i) % capacity_;
        const auto& s = buf_[idx];
        timestamps[i]             = static_cast<float>(s.timestampMs);
        throttle[i]                = s.throttle;
        front_left_rpm[i]          = s.front_left_rpm;
        front_right_rpm[i]         = s.front_right_rpm;
        rear_left_rpm[i]           = s.rear_left_rpm;
        rear_right_rpm[i]          = s.rear_right_rpm;
        rear_left_target_rpm[i]    = s.rear_left_target_rpm;
        rear_right_target_rpm[i]   = s.rear_right_target_rpm;
        rear_left_pwm[i]           = s.rear_left_pwm;
        rear_right_pwm[i]          = s.rear_right_pwm;
        real_rpm[i]                = s.real_rpm;
        rear_left_slip[i]          = s.rear_left_slip;
        rear_right_slip[i]         = s.rear_right_slip;
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

    // BluetoothSerialClient's writer thread issues WriteFile on this same
    // HANDLE from another thread. Serial/Bluetooth-SPP drivers commonly
    // serialize all I/O requests on a device object, so a long blocking
    // ReadFile call here directly delays queued writes. Force a short, flat
    // per-call timeout (independent of requested buffer size) so this thread
    // never holds the handle for long, regardless of how connect() tuned it.
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout         = 0;
    timeouts.ReadTotalTimeoutConstant    = 15;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.WriteTotalTimeoutConstant   = 100;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(h, &timeouts);

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

                const auto& p = msg.payload.log;

                // Reference wheel speed for slip: fastest (least loaded) front wheel.
                const float realRpmRaw = static_cast<float>(std::max(p.front_left_rpm, p.front_right_rpm));

                LogSample sample{};
                sample.timestampMs          = frameStartMs;
                sample.throttle             = p.throttle / 255.0f;
                sample.front_left_rpm       = p.front_left_rpm  / 255.0f;
                sample.front_right_rpm      = p.front_right_rpm / 255.0f;
                sample.rear_left_rpm        = p.rear_left_rpm  / 255.0f;
                sample.rear_right_rpm       = p.rear_right_rpm / 255.0f;
                sample.rear_left_target_rpm  = p.rear_left_target_rpm  / 255.0f;
                sample.rear_right_target_rpm = p.rear_right_target_rpm / 255.0f;
                sample.rear_left_pwm        = p.rear_left_pwm  / 255.0f;
                sample.rear_right_pwm       = p.rear_right_pwm / 255.0f;
                sample.real_rpm             = realRpmRaw / 255.0f;
                sample.rear_left_slip       = realRpmRaw > 0.0f ? p.rear_left_rpm  / realRpmRaw : 0.0f;
                sample.rear_right_slip      = realRpmRaw > 0.0f ? p.rear_right_rpm / realRpmRaw : 0.0f;

                buffer_.push(sample);

                state = State::WaitSof;
            }
        }
    }
}

}  // namespace bluestick
