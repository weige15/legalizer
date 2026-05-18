#pragma once

#include <string>

#include "placement_model.hpp"

namespace legalizer {

class GpParser {
 public:
  static bool parseFile(const std::string& path, Design& design, std::string& error);
};

}  // namespace legalizer
