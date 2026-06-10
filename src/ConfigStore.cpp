#include "bluestick/ConfigStore.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace bluestick {

namespace {

ActionType parseActionType(const std::string& value) {
  if (value == "toggle_cc") {
    return ActionType::ToggleCc;
  }
  if (value == "inc_cc") {
    return ActionType::IncCc;
  }
  if (value == "dec_cc") {
    return ActionType::DecCc;
  }
  if (value == "set_throttle") {
    return ActionType::SetThrottle;
  }
  return ActionType::ToggleTc;
}

const char* actionTypeToString(ActionType type) {
  switch (type) {
    case ActionType::ToggleTc:
      return "toggle_tc";
    case ActionType::ToggleCc:
      return "toggle_cc";
    case ActionType::IncCc:
      return "inc_cc";
    case ActionType::DecCc:
      return "dec_cc";
    case ActionType::SetThrottle:
      return "set_throttle";
  }
  return "toggle_tc";
}

}  // namespace

std::optional<std::vector<ActionBinding>> ConfigStore::loadBindings(const std::string& path,
                                                                    std::string& error) {
  error.clear();
  std::ifstream file(path);
  if (!file.is_open()) {
    error = "Could not open file for read: " + path;
    return std::nullopt;
  }

  nlohmann::json root;
  try {
    file >> root;
  } catch (const std::exception& ex) {
    error = std::string("JSON parse error: ") + ex.what();
    return std::nullopt;
  }

  if (!root.contains("bindings") || !root["bindings"].is_array()) {
    error = "Invalid config: missing bindings array";
    return std::nullopt;
  }

  std::vector<ActionBinding> bindings;
  for (const auto& item : root["bindings"]) {
    ActionBinding binding;
    binding.id = item.value("id", 0u);
    binding.enabled = item.value("enabled", true);

    const std::string sourceType = item.value("sourceType", "button");
    binding.sourceType = (sourceType == "axis") ? MappingSourceType::Axis : MappingSourceType::Button;

    binding.sourceIndex = item.value("sourceIndex", 0);
    binding.action = parseActionType(item.value("action", "toggle_tc"));
    binding.axisThreshold = item.value("axisThreshold", 0.5f);
    binding.axisDeltaThreshold = item.value("axisDeltaThreshold", 0.05f);
    binding.axisMinIntervalMs = item.value("axisMinIntervalMs", 50);

    bindings.push_back(binding);
  }

  return bindings;
}

bool ConfigStore::saveBindings(const std::string& path,
                              const std::vector<ActionBinding>& bindings,
                              std::string& error) {
  error.clear();

  nlohmann::json root;
  root["bindings"] = nlohmann::json::array();

  for (const ActionBinding& binding : bindings) {
    root["bindings"].push_back({
        {"id", binding.id},
        {"enabled", binding.enabled},
        {"sourceType", binding.sourceType == MappingSourceType::Axis ? "axis" : "button"},
        {"sourceIndex", binding.sourceIndex},
        {"action", actionTypeToString(binding.action)},
        {"axisThreshold", binding.axisThreshold},
        {"axisDeltaThreshold", binding.axisDeltaThreshold},
        {"axisMinIntervalMs", binding.axisMinIntervalMs},
    });
  }

  const std::filesystem::path outputPath(path);
  std::filesystem::create_directories(outputPath.parent_path());

  std::ofstream file(path);
  if (!file.is_open()) {
    error = "Could not open file for write: " + path;
    return false;
  }

  try {
    file << root.dump(2);
  } catch (const std::exception& ex) {
    error = std::string("JSON write error: ") + ex.what();
    return false;
  }

  return true;
}

}  // namespace bluestick
