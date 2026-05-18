#pragma once

#include <string>

#include "placement_model.hpp"

namespace legalizer {

class TclWriter {
 public:
  static bool writeFile(const std::string& path, const Design& design,
                        std::string& error);
};

}  // namespace legalizer
