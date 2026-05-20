#ifndef DENSITY_ESTIMATOR_H
#define DENSITY_ESTIMATOR_H

#include "placement_model.h"

#include <vector>

namespace legalizer {

struct DensitySummary {
    int total_bins = 0;
    int overflow_bins = 0;
    double dor_percent = 0.0;
};

DensitySummary computeDensitySummary(const PlacementModel& model,
                                     const std::vector<Point>& placements,
                                     double threshold);

ValidationResult validatePlacement(const PlacementModel& model, const std::vector<Point>& placements,
                                   const std::vector<RowInterval>& intervals, double alpha,
                                   double threshold);

double estimateLocalDensityPenalty(const PlacementModel& model,
                                   const std::vector<Point>& placements,
                                   const std::vector<bool>& placed,
                                   size_t trial_cell_id,
                                   Point trial_ll,
                                   double threshold);

}  // namespace legalizer

#endif
