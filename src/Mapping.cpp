#include "bluestick/Mapping.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

namespace bluestick {

void MappingEngine::setRules(const std::vector<MappingRule>& rules) {
  rules_ = rules;
}

const std::vector<MappingRule>& MappingEngine::rules() const {
  return rules_;
}

std::vector<std::string> MappingEngine::evaluate(const InputSnapshot& previous,
                                                 const InputSnapshot& current,
                                                 std::chrono::steady_clock::time_point now) {
  std::vector<std::string> messages;
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
        messages.push_back(renderTemplate(rule, current.buttons[idx] ? 1.0f : 0.0f));
      }
      continue;
    }

    if (rule.sourceIndex < 0 || rule.sourceIndex >= static_cast<int>(AxisCount)) {
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
      messages.push_back(renderTemplate(rule, currentValue));
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

std::string MappingEngine::renderTemplate(const MappingRule& rule, float value) {
  std::string rendered = rule.messageTemplate;

  const std::string source = sourceLabel(rule);

  const size_t sourcePos = rendered.find("{source}");
  if (sourcePos != std::string::npos) {
    rendered.replace(sourcePos, 8, source);
  }

  const size_t valuePos = rendered.find("{value}");
  if (valuePos != std::string::npos) {
    rendered.replace(valuePos, 7, std::format("{:.3f}", value));
  }

  return rendered;
}

}  // namespace bluestick
