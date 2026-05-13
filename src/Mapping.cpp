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

Message_t buildMessage(const MappingRule& rule, float value) {
  Message_t message{};
  switch (rule.messageType) {
    case MappingMessageType::SetThrottle:
      message.set_throttle.type = MSG_TYPE_SET_THROTTLE;
      message.set_throttle.throttle = mapThrottle(value);
      break;
    case MappingMessageType::ToggleTc:
      message.toggle_tc.type = MSG_TYPE_TOGGLE_TC;
      break;
  }
  return message;
}

}  // namespace

void MappingEngine::setRules(const std::vector<MappingRule>& rules) {
  rules_ = rules;
}

const std::vector<MappingRule>& MappingEngine::rules() const {
  return rules_;
}

std::vector<Message_t> MappingEngine::evaluate(const InputSnapshot& previous,
                                               const InputSnapshot& current,
                                               std::chrono::steady_clock::time_point now) {
  std::vector<Message_t> messages;
  if (!current.connected) {
    return messages;
  }

  for (size_t i = 0; i < rules_.size(); ++i) {
    const MappingRule& rule = rules_[i];
    if (!rule.enabled) {
      continue;
    }

    if (rule.sourceType == MappingSourceType::Button) {
      if (rule.sourceIndex < 0 || rule.sourceIndex >= static_cast<int>(ButtonCount)) {
        continue;
      }

      const size_t idx = static_cast<size_t>(rule.sourceIndex);
      if (previous.buttons[idx] != current.buttons[idx]) {
        if (current.buttons[idx] && rule.messageType == MappingMessageType::ToggleTc) {
          messages.push_back(buildMessage(rule, 1.0f));
        }
      }
      continue;
    }

    if (rule.sourceIndex < 0 || rule.sourceIndex >= static_cast<int>(AxisCount)) {
      continue;
    }

    if (rule.messageType != MappingMessageType::SetThrottle) {
      continue;
    }

    const size_t idx = static_cast<size_t>(rule.sourceIndex);
    const float currentValue = current.axes[idx];
    const float previousSentValue = lastAxisSentValues_[idx];
    const float delta = std::fabs(currentValue - previousSentValue);

    const auto iter = lastAxisSentTimes_.find(static_cast<int>(idx));
    const bool hasSentBefore = iter != lastAxisSentTimes_.end();
    const bool intervalOk = !hasSentBefore ||
                            std::chrono::duration_cast<std::chrono::milliseconds>(now - iter->second).count() >=
                                rule.axisMinIntervalMs;

    if (delta >= rule.axisDeltaThreshold && intervalOk) {
      messages.push_back(buildMessage(rule, currentValue));
      lastAxisSentValues_[idx] = currentValue;
      lastAxisSentTimes_[static_cast<int>(idx)] = now;
    }
  }

  return messages;
}

std::string MappingEngine::sourceLabel(const MappingRule& rule) {
  if (rule.sourceType == MappingSourceType::Button) {
    if (rule.sourceIndex >= 0 && rule.sourceIndex < static_cast<int>(ButtonCount)) {
      return GamepadInput::buttonName(static_cast<GamepadButton>(rule.sourceIndex));
    }
    return "InvalidButton";
  }

  if (rule.sourceIndex >= 0 && rule.sourceIndex < static_cast<int>(AxisCount)) {
    return GamepadInput::axisName(static_cast<GamepadAxis>(rule.sourceIndex));
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
