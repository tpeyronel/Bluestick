#include "bluestick/ConfigStore.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace bluestick {

std::optional<std::vector<MappingRule>> ConfigStore::loadMappings(const std::string& path,
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

  if (!root.contains("mappings") || !root["mappings"].is_array()) {
    error = "Invalid config: missing mappings array";
    return std::nullopt;
  }

  std::vector<MappingRule> rules;
  for (const auto& item : root["mappings"]) {
    MappingRule rule;
    rule.enabled = item.value("enabled", true);

    const std::string sourceType = item.value("sourceType", "button");
    rule.sourceType = (sourceType == "axis") ? MappingSourceType::Axis : MappingSourceType::Button;

    rule.sourceIndex = item.value("sourceIndex", 0);
    rule.messageTemplate = item.value("messageTemplate", "{source}:{value}");
    rule.axisDeltaThreshold = item.value("axisDeltaThreshold", 0.05f);
    rule.axisMinIntervalMs = item.value("axisMinIntervalMs", 50);

    rules.push_back(rule);
  }

  return rules;
}

bool ConfigStore::saveMappings(const std::string& path,
                               const std::vector<MappingRule>& rules,
                               std::string& error) {
  error.clear();

  nlohmann::json root;
  root["mappings"] = nlohmann::json::array();

  for (const MappingRule& rule : rules) {
    root["mappings"].push_back({
        {"enabled", rule.enabled},
        {"sourceType", rule.sourceType == MappingSourceType::Axis ? "axis" : "button"},
        {"sourceIndex", rule.sourceIndex},
        {"messageTemplate", rule.messageTemplate},
        {"axisDeltaThreshold", rule.axisDeltaThreshold},
        {"axisMinIntervalMs", rule.axisMinIntervalMs},
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
