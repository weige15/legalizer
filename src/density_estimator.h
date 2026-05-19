#ifndef DENSITY_ESTIMATOR_H
#define DENSITY_ESTIMATOR_H

#include "placement_model.h"

#include <string>
#include <vector>

namespace legalizer {

struct DensityResult {
  int total_grids = 0;
  int overflow_grids = 0;
  double dor = 0.0;
};

class DensityGrid {
 public:
  explicit DensityGrid(const PlacementModel &model);

  void clearMovableArea();
  void addRect(const Rect &rect, double sign = 1.0);
  DensityResult compute(double threshold_percent) const;
  double trialPenalty(const Rect &rect, double threshold_percent) const;

 private:
  Rect gridRect(int gx, int gy) const;
  std::size_t index(int gx, int gy) const;

  const PlacementModel &model_;
  Coord grid_size_ = 0;
  int cols_ = 0;
  int rows_ = 0;
  std::vector<double> movable_area_;
  std::vector<bool> excluded_;
};

DensityResult computeFinalDensity(const PlacementModel &model, double threshold_percent);

}  // namespace legalizer

#endif
