#ifndef LEGALIZER_H
#define LEGALIZER_H

#include "placement_model.h"

#include <string>
#include <vector>

namespace legalizer {

struct LegalizationResult {
    std::vector<Point> placements;
    Metrics metrics;
};

LegalizationResult legalize(const PlacementModel& model, const std::vector<RowInterval>& intervals,
                            double alpha, double threshold);

std::vector<Point> solveRowInterval(const PlacementModel& model, const RowInterval& interval,
                                    const std::vector<size_t>& cell_ids);

}  // namespace legalizer

#endif
