#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER)
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif

enum MessageType : uint8_t {
    MSG_TYPE_SET_THROTTLE = 0,
    MSG_TYPE_TOGGLE_TC = 1,
    MSG_TYPE_TOGGLE_CC = 2,
    MSG_TYPE_INC_CC = 3,
    MSG_TYPE_DEC_CC = 4,
    MSG_TYPE_SET_MOTOR_PID_CONFIG = 5,
};

struct PACKED MessageSetThrottle {
    enum MessageType type;
    uint8_t throttle;
};

struct PACKED MessageToggleTc {
    enum MessageType type;
};

struct PACKED MessageToggleCc {
    enum MessageType type;
};

struct PACKED MessageIncCc {
    enum MessageType type;
};

struct PACKED MessageDecCc {
    enum MessageType type;
};

// Sent individually (not part of Message_t union) — 14 bytes.
struct PACKED MessageSetMotorPidConfig {
    enum MessageType type;
    uint8_t motor_id;
    float Kp;
    float Ki;
    float Kd;
};

typedef union PACKED Message_t {
    enum MessageType type;
    struct MessageSetThrottle set_throttle;
    struct MessageToggleTc toggle_tc;
    struct MessageToggleCc toggle_cc;
    struct MessageIncCc inc_cc;
    struct MessageDecCc dec_cc;
} Message_t;

constexpr size_t MESSAGE_SIZE = sizeof(Message_t);

// ---- Incoming messages (device → PC) ----

enum MessageOutType : uint8_t {
    MSG_OUT_TYPE_LOG = 0,
};

struct PACKED MessageOutLogPayload {
    // 0.0 to 1.0
    uint8_t throttle;

    // 0 RPM to 255 RPM
    uint8_t front_right_rpm;
    uint8_t front_left_rpm;
    uint8_t rear_right_rpm;
    uint8_t rear_left_rpm;
    uint8_t rear_right_target_rpm;
    uint8_t rear_left_target_rpm;

    // 0.0 to 1.0
    uint8_t rear_left_pwm;
    uint8_t rear_right_pwm;
};

union PACKED MessageOutPayload {
    MessageOutLogPayload log;
};

#define START_OF_FRAME_MARKER 0xAA

typedef struct PACKED MessageOut_t {
    uint8_t sof;
    MessageOutType type;
    MessageOutPayload payload;
} MessageOut_t;

constexpr size_t MESSAGE_OUT_SIZE = sizeof(MessageOut_t);

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
