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
};

struct PACKED MessageSetThrottle {
    enum MessageType type;
    uint8_t throttle;
};

struct PACKED MessageToggleTc {
    enum MessageType type;
};

typedef union PACKED Message_t {
    enum MessageType type;
    struct MessageSetThrottle set_throttle;
    struct MessageToggleTc toggle_tc;
} Message_t;

constexpr size_t MESSAGE_SIZE = sizeof(Message_t);

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
