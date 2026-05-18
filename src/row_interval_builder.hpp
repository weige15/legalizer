#pragma once

#include <string>
#include <vector>

#include "placement_model.hpp"

namespace legalizer {

struct RowInterval {
  int64_t x_min = 0;
  int64_t x_max = 0;
};

struct LegalRow {
  int64_t y = 0;
  std::vector<RowInterval> free_intervals;
};

class RowIntervalBuilder {
 public:
  static bool build(const Design& design, std::vector<LegalRow>& rows,
                    std::string& error);
};

bool intervalContains(const RowInterval& interval, int64_t x, int64_t width);

}  // namespace legalizer
