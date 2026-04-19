#pragma once

#include <optional>
#include <string>
#include <vector>

#include "bluestick/Mapping.hpp"

namespace bluestick {

class ConfigStore {
public:
  static std::optional<std::vector<MappingRule>> loadMappings(const std::string& path,
                                                              std::string& error);
  static bool saveMappings(const std::string& path,
                           const std::vector<MappingRule>& rules,
                           std::string& error);
};

}  // namespace bluestick
