#ifndef ROW_INTERVAL_BUILDER_H
#define ROW_INTERVAL_BUILDER_H

#include "placement_model.h"

#include <vector>

namespace legalizer {

std::vector<RowInterval> buildRowIntervals(const PlacementModel& model);

}  // namespace legalizer

#endif
