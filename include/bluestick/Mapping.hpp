#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "protocol.h"

#include "bluestick/GamepadInput.hpp"

namespace bluestick {

enum class MappingSourceType {
  Button,
  Axis
};

enum class ActionType {
  ToggleTc,
  ToggleCc,
  IncCc,
  DecCc,
  SetThrottle
};

struct ActionBinding {
  uint32_t id = 0;
  bool enabled = true;
  MappingSourceType sourceType = MappingSourceType::Button;
  int sourceIndex = 0;
  ActionType action = ActionType::ToggleTc;
  float axisThreshold = 0.5f;
  float axisDeltaThreshold = 0.05f;
  int axisMinIntervalMs = 50;
};

class MappingEngine {
public:
  void setBindings(const std::vector<ActionBinding>& bindings);
  const std::vector<ActionBinding>& bindings() const;

  std::vector<Message_t> evaluate(const InputSnapshot& previous,
                                  const InputSnapshot& current,
                                  std::chrono::steady_clock::time_point now);

  static const char* actionLabel(ActionType action);
  static std::string sourceLabel(const ActionBinding& binding);
  static std::vector<std::string> allButtonSources();
  static std::vector<std::string> allAxisSources();

private:
  std::vector<ActionBinding> bindings_;
  std::unordered_map<uint32_t, float> lastAxisSentValues_;
  std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> lastAxisSentTimes_;
};

}  // namespace bluestick
