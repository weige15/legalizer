#ifndef ROW_INTERVAL_BUILDER_H
#define ROW_INTERVAL_BUILDER_H

#include "placement_model.h"

#include <string>
#include <vector>

namespace legalizer {

struct RowInterval {
  Coord llx = 0;
  Coord urx = 0;
};

struct SiteRow {
  int index = 0;
  Coord y = 0;
  std::vector<RowInterval> intervals;
};

struct RowBuildResult {
  bool ok = false;
  std::vector<SiteRow> rows;
  std::string error;
};

RowBuildResult buildRowIntervals(const PlacementModel &model);

}  // namespace legalizer

#endif
