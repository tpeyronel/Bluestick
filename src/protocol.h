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

struct PACKED MessageOutLog {
    MessageOutType type;
    uint8_t throttle;
    uint8_t rear_left_pwm;
    uint8_t rear_right_pwm;
    uint8_t rear_left_slip;
    uint8_t rear_right_slip;
};

constexpr size_t MESSAGE_OUT_LOG_SIZE = sizeof(MessageOutLog);

typedef union PACKED MessageOut_t {
    MessageOutType type;
    MessageOutLog log;
} MessageOut_t;

constexpr size_t MESSAGE_OUT_SIZE = sizeof(MessageOut_t);

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
