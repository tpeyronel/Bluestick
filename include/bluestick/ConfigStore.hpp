#pragma once

#include <optional>
#include <string>
#include <vector>

#include "bluestick/Mapping.hpp"

namespace bluestick {

class ConfigStore {
public:
  static std::optional<std::vector<ActionBinding>> loadBindings(const std::string& path,
                                                                std::string& error);
  static bool saveBindings(const std::string& path,
                          const std::vector<ActionBinding>& bindings,
                          std::string& error);
};

}  // namespace bluestick
