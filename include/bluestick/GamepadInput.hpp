#pragma once

#include <array>
#include <string>

namespace bluestick {

enum class GamepadButton {
  A,
  B,
  X,
  Y,
  LB,
  RB,
  Back,
  Start,
  LStick,
  RStick,
  DpadUp,
  DpadDown,
  DpadLeft,
  DpadRight,
  Count
};

enum class GamepadAxis {
  LX,
  LY,
  RX,
  RY,
  LT,
  RT,
  Count
};

constexpr size_t ButtonCount = static_cast<size_t>(GamepadButton::Count);
constexpr size_t AxisCount = static_cast<size_t>(GamepadAxis::Count);

struct InputSnapshot {
  bool connected = false;
  std::array<bool, ButtonCount> buttons{};
  std::array<float, AxisCount> axes{};
};

class GamepadInput {
public:
  InputSnapshot poll();

  static const char* buttonName(GamepadButton button);
  static const char* axisName(GamepadAxis axis);
};

}  // namespace bluestick
