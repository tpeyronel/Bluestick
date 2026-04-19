#pragma once

#include <array>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include "bluestick/GamepadInput.hpp"

namespace bluestick {

enum class MappingSourceType {
  Button,
  Axis
};

struct MappingRule {
  bool enabled = true;
  MappingSourceType sourceType = MappingSourceType::Button;
  int sourceIndex = 0;
  std::string messageTemplate = "{source}:{value}";
  float axisDeltaThreshold = 0.05f;
  int axisMinIntervalMs = 50;
};

class MappingEngine {
public:
  void setRules(const std::vector<MappingRule>& rules);
  const std::vector<MappingRule>& rules() const;

  std::vector<std::string> evaluate(const InputSnapshot& previous,
                                    const InputSnapshot& current,
                                    std::chrono::steady_clock::time_point now);

  static std::string sourceLabel(const MappingRule& rule);
  static std::vector<std::string> allButtonSources();
  static std::vector<std::string> allAxisSources();

private:
  std::vector<MappingRule> rules_;
  std::array<float, AxisCount> lastAxisSentValues_{};
  std::unordered_map<int, std::chrono::steady_clock::time_point> lastAxisSentTimes_;

  static std::string renderTemplate(const MappingRule& rule, float value);
};

}  // namespace bluestick
