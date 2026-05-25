#ifndef DENSITY_ESTIMATOR_H
#define DENSITY_ESTIMATOR_H

#include <vector>

#include "placement_model.h"

namespace legalizer {

struct Metrics {
  double averageDisplacementMicron = 0.0;
  double dorPercent = 0.0;
  double quality = 0.0;
  int totalGrids = 0;
  int overflowGrids = 0;
};

struct DensityGridInfo {
  int index = 0;
  Rect rect;
  double densityPercent = 0.0;
  bool overflow = false;
};

Metrics evaluateMetrics(const PlacementModel &model, double alpha, double threshold);
std::vector<DensityGridInfo> computeDensityGrids(const PlacementModel &model,
                                                 double threshold);

}  // namespace legalizer

#endif
