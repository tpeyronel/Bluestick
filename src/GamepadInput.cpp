#include "bluestick/GamepadInput.hpp"

#include <algorithm>
#include <cmath>

#include <Windows.h>
#include <Xinput.h>

namespace bluestick {

namespace {

float normalizeStick(SHORT value, SHORT deadZone) {
  const int sign = value < 0 ? -1 : 1;
  const float absValue = static_cast<float>(std::abs(value));
  if (absValue <= static_cast<float>(deadZone)) {
    return 0.0f;
  }

  const float clamped = std::min(absValue, 32767.0f);
  const float normalized = (clamped - static_cast<float>(deadZone)) /
                           (32767.0f - static_cast<float>(deadZone));
  return normalized * static_cast<float>(sign);
}

float normalizeTrigger(BYTE value) {
  constexpr float maxTrigger = 255.0f;
  return static_cast<float>(value) / maxTrigger;
}

}  // namespace

InputSnapshot GamepadInput::poll() {
  InputSnapshot snapshot;

  XINPUT_STATE state{};
  if (XInputGetState(0, &state) != ERROR_SUCCESS) {
    return snapshot;
  }

  snapshot.connected = true;

  const WORD buttons = state.Gamepad.wButtons;
  snapshot.buttons[static_cast<size_t>(GamepadButton::A)] = (buttons & XINPUT_GAMEPAD_A) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::B)] = (buttons & XINPUT_GAMEPAD_B) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::X)] = (buttons & XINPUT_GAMEPAD_X) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::Y)] = (buttons & XINPUT_GAMEPAD_Y) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::LB)] = (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::RB)] = (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::Back)] = (buttons & XINPUT_GAMEPAD_BACK) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::Start)] = (buttons & XINPUT_GAMEPAD_START) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::LStick)] = (buttons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::RStick)] = (buttons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::DpadUp)] = (buttons & XINPUT_GAMEPAD_DPAD_UP) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::DpadDown)] = (buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::DpadLeft)] = (buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
  snapshot.buttons[static_cast<size_t>(GamepadButton::DpadRight)] = (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;

  snapshot.axes[static_cast<size_t>(GamepadAxis::LX)] =
      normalizeStick(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
  snapshot.axes[static_cast<size_t>(GamepadAxis::LY)] =
      normalizeStick(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
  snapshot.axes[static_cast<size_t>(GamepadAxis::RX)] =
      normalizeStick(state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
  snapshot.axes[static_cast<size_t>(GamepadAxis::RY)] =
      normalizeStick(state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
  snapshot.axes[static_cast<size_t>(GamepadAxis::LT)] = normalizeTrigger(state.Gamepad.bLeftTrigger);
  snapshot.axes[static_cast<size_t>(GamepadAxis::RT)] = normalizeTrigger(state.Gamepad.bRightTrigger);

  return snapshot;
}

const char* GamepadInput::buttonName(GamepadButton button) {
  switch (button) {
    case GamepadButton::A:
      return "A";
    case GamepadButton::B:
      return "B";
    case GamepadButton::X:
      return "X";
    case GamepadButton::Y:
      return "Y";
    case GamepadButton::LB:
      return "LB";
    case GamepadButton::RB:
      return "RB";
    case GamepadButton::Back:
      return "Back";
    case GamepadButton::Start:
      return "Start";
    case GamepadButton::LStick:
      return "LStick";
    case GamepadButton::RStick:
      return "RStick";
    case GamepadButton::DpadUp:
      return "DPadUp";
    case GamepadButton::DpadDown:
      return "DPadDown";
    case GamepadButton::DpadLeft:
      return "DPadLeft";
    case GamepadButton::DpadRight:
      return "DPadRight";
    case GamepadButton::Count:
      break;
  }
  return "Unknown";
}

const char* GamepadInput::axisName(GamepadAxis axis) {
  switch (axis) {
    case GamepadAxis::LX:
      return "LX";
    case GamepadAxis::LY:
      return "LY";
    case GamepadAxis::RX:
      return "RX";
    case GamepadAxis::RY:
      return "RY";
    case GamepadAxis::LT:
      return "LT";
    case GamepadAxis::RT:
      return "RT";
    case GamepadAxis::Count:
      break;
  }
  return "Unknown";
}

}  // namespace bluestick
