#include "bluestick/Mapping.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace bluestick {

namespace {

uint8_t mapThrottle(float value) {
  const float clamped = std::clamp(value, 0.0f, 1.0f);
  const float scaled = clamped * 255.0f;
  return static_cast<uint8_t>(std::lround(scaled));
}

bool isNearZero(float value) {
  return value <= 0.01f;
}

uint32_t bindingKey(const ActionBinding& binding, size_t index) {
  return binding.id != 0 ? binding.id : static_cast<uint32_t>(index + 1);
}

bool isBinaryAction(ActionType action) {
  return action != ActionType::SetThrottle;
}

MessageType actionToMessageType(ActionType action) {
  switch (action) {
    case ActionType::ToggleTc:
      return MSG_TYPE_TOGGLE_TC;
    case ActionType::ToggleCc:
      return MSG_TYPE_TOGGLE_CC;
    case ActionType::IncCc:
      return MSG_TYPE_INC_CC;
    case ActionType::DecCc:
      return MSG_TYPE_DEC_CC;
    case ActionType::SetThrottle:
      return MSG_TYPE_SET_THROTTLE;
  }
  return MSG_TYPE_TOGGLE_TC;
}

Message_t buildBinaryMessage(ActionType action) {
  Message_t message{};
  message.type = actionToMessageType(action);
  return message;
}

Message_t buildThrottleMessage(float value) {
  Message_t message{};
  message.set_throttle.type = MSG_TYPE_SET_THROTTLE;
  message.set_throttle.throttle = mapThrottle(value);
  return message;
}

}  // namespace

void MappingEngine::setBindings(const std::vector<ActionBinding>& bindings) {
  bindings_ = bindings;
}

const std::vector<ActionBinding>& MappingEngine::bindings() const {
  return bindings_;
}

std::vector<Message_t> MappingEngine::evaluate(const InputSnapshot& previous,
                                               const InputSnapshot& current,
                                               std::chrono::steady_clock::time_point now) {
  std::vector<Message_t> messages;
  if (!current.connected) {
    return messages;
  }

  for (size_t i = 0; i < bindings_.size(); ++i) {
    const ActionBinding& binding = bindings_[i];
    if (!binding.enabled) {
      continue;
    }

    const uint32_t key = bindingKey(binding, i);

    if (binding.sourceType == MappingSourceType::Button) {
      if (binding.sourceIndex < 0 || binding.sourceIndex >= static_cast<int>(ButtonCount)) {
        continue;
      }

      const size_t idx = static_cast<size_t>(binding.sourceIndex);
      if (previous.buttons[idx] != current.buttons[idx]) {
        if (isBinaryAction(binding.action)) {
          if (current.buttons[idx]) {
            messages.push_back(buildBinaryMessage(binding.action));
          }
        } else {
          // Continuous action fed by a button acts as binary analog: released=0.0, pressed=1.0.
          messages.push_back(buildThrottleMessage(current.buttons[idx] ? 1.0f : 0.0f));
        }
      }
      continue;
    }

    if (binding.sourceIndex < 0 || binding.sourceIndex >= static_cast<int>(AxisCount)) {
      continue;
    }

    const size_t idx = static_cast<size_t>(binding.sourceIndex);
    const float currentValue = current.axes[idx];
    if (isBinaryAction(binding.action)) {
      const bool previousActive = std::fabs(previous.axes[idx]) >= binding.axisThreshold;
      const bool currentActive = std::fabs(currentValue) >= binding.axisThreshold;
      if (!previousActive && currentActive) {
        const auto iter = lastAxisSentTimes_.find(key);
        const bool hasSentBefore = iter != lastAxisSentTimes_.end();
        const bool intervalOk = !hasSentBefore ||
                                std::chrono::duration_cast<std::chrono::milliseconds>(now - iter->second).count() >=
                                    binding.axisMinIntervalMs;
        if (intervalOk) {
          messages.push_back(buildBinaryMessage(binding.action));
          lastAxisSentTimes_[key] = now;
        }
      }
      continue;
    }

    const float previousSentValue = lastAxisSentValues_[key];
    const float delta = std::fabs(currentValue - previousSentValue);

    const auto iter = lastAxisSentTimes_.find(key);
    const bool hasSentBefore = iter != lastAxisSentTimes_.end();
    const bool intervalOk = !hasSentBefore ||
                            std::chrono::duration_cast<std::chrono::milliseconds>(now - iter->second).count() >=
                                binding.axisMinIntervalMs;

    if (isNearZero(currentValue) && previousSentValue > 0.0f) {
      messages.push_back(buildThrottleMessage(0.0f));
      lastAxisSentValues_[key] = 0.0f;
      lastAxisSentTimes_[key] = now;
      continue;
    }

    if (delta >= binding.axisDeltaThreshold && intervalOk) {
      messages.push_back(buildThrottleMessage(currentValue));
      lastAxisSentValues_[key] = currentValue;
      lastAxisSentTimes_[key] = now;
    }
  }

  return messages;
}

const char* MappingEngine::actionLabel(ActionType action) {
  switch (action) {
    case ActionType::ToggleTc:
      return "ToggleTc";
    case ActionType::ToggleCc:
      return "ToggleCc";
    case ActionType::IncCc:
      return "IncCc";
    case ActionType::DecCc:
      return "DecCc";
    case ActionType::SetThrottle:
      return "SetThrottle";
  }
  return "Unknown";
}

std::string MappingEngine::sourceLabel(const ActionBinding& binding) {
  if (binding.sourceType == MappingSourceType::Button) {
    if (binding.sourceIndex >= 0 && binding.sourceIndex < static_cast<int>(ButtonCount)) {
      return GamepadInput::buttonName(static_cast<GamepadButton>(binding.sourceIndex));
    }
    return "InvalidButton";
  }

  if (binding.sourceIndex >= 0 && binding.sourceIndex < static_cast<int>(AxisCount)) {
    return GamepadInput::axisName(static_cast<GamepadAxis>(binding.sourceIndex));
  }

  return "InvalidAxis";
}

std::vector<std::string> MappingEngine::allButtonSources() {
  std::vector<std::string> result;
  result.reserve(ButtonCount);
  for (int i = 0; i < static_cast<int>(ButtonCount); ++i) {
    result.push_back(GamepadInput::buttonName(static_cast<GamepadButton>(i)));
  }
  return result;
}

std::vector<std::string> MappingEngine::allAxisSources() {
  std::vector<std::string> result;
  result.reserve(AxisCount);
  for (int i = 0; i < static_cast<int>(AxisCount); ++i) {
    result.push_back(GamepadInput::axisName(static_cast<GamepadAxis>(i)));
  }
  return result;
}

}  // namespace bluestick
