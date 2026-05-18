#include "density_estimator.hpp"

#include <algorithm>

namespace legalizer {

DensityEstimator::DensityEstimator(const Design& design, double threshold)
    : design_(design), threshold_(threshold) {
  grid_size_ = std::max<int64_t>(1, 10 * design_.dbu_per_micron);
  cols_ = ceilDiv(rectWidth(design_.die), grid_size_);
  rows_ = ceilDiv(rectHeight(design_.die), grid_size_);

  for (const Obstacle& obstacle : design_.obstacles) {
    if (obstacle.type == InstanceType::Macro) {
      addArea(intersection(obstacle.rect, design_.die), true);
    }
  }
}

uint64_t DensityEstimator::key(int64_t gx, int64_t gy) const {
  return (static_cast<uint64_t>(gy) << 32) ^ static_cast<uint64_t>(gx);
}

Rect DensityEstimator::gridRect(int64_t gx, int64_t gy) const {
  const int64_t x0 = design_.die.x_min + gx * grid_size_;
  const int64_t y0 = design_.die.y_min + gy * grid_size_;
  return intersection(Rect{x0, y0, x0 + grid_size_, y0 + grid_size_},
                      design_.die);
}

int64_t DensityEstimator::gridArea(int64_t gx, int64_t gy) const {
  return rectArea(gridRect(gx, gy));
}

void DensityEstimator::addArea(const Rect& rect, bool macro) {
  if (!isValidRect(rect)) return;
  int64_t gx0 = floorDiv(rect.x_min - design_.die.x_min, grid_size_);
  int64_t gy0 = floorDiv(rect.y_min - design_.die.y_min, grid_size_);
  int64_t gx1 = floorDiv(rect.x_max - 1 - design_.die.x_min, grid_size_);
  int64_t gy1 = floorDiv(rect.y_max - 1 - design_.die.y_min, grid_size_);
  gx0 = clampInt64(gx0, 0, std::max<int64_t>(0, cols_ - 1));
  gx1 = clampInt64(gx1, 0, std::max<int64_t>(0, cols_ - 1));
  gy0 = clampInt64(gy0, 0, std::max<int64_t>(0, rows_ - 1));
  gy1 = clampInt64(gy1, 0, std::max<int64_t>(0, rows_ - 1));

  for (int64_t gy = gy0; gy <= gy1; ++gy) {
    for (int64_t gx = gx0; gx <= gx1; ++gx) {
      const Rect overlap = intersection(rect, gridRect(gx, gy));
      if (!isValidRect(overlap)) continue;
      Grid& grid = grids_[key(gx, gy)];
      if (macro) {
        grid.macro_area += rectArea(overlap);
      } else {
        grid.movable_area += rectArea(overlap);
      }
    }
  }
}

double DensityEstimator::scoreCandidate(const Rect& rect) const {
  const Rect clipped = intersection(rect, design_.die);
  if (!isValidRect(clipped)) return 1000000.0;

  int64_t gx0 = floorDiv(clipped.x_min - design_.die.x_min, grid_size_);
  int64_t gy0 = floorDiv(clipped.y_min - design_.die.y_min, grid_size_);
  int64_t gx1 = floorDiv(clipped.x_max - 1 - design_.die.x_min, grid_size_);
  int64_t gy1 = floorDiv(clipped.y_max - 1 - design_.die.y_min, grid_size_);
  gx0 = clampInt64(gx0, 0, std::max<int64_t>(0, cols_ - 1));
  gx1 = clampInt64(gx1, 0, std::max<int64_t>(0, cols_ - 1));
  gy0 = clampInt64(gy0, 0, std::max<int64_t>(0, rows_ - 1));
  gy1 = clampInt64(gy1, 0, std::max<int64_t>(0, rows_ - 1));

  double weighted_overflow = 0.0;
  double touched_weight = 0.0;
  for (int64_t gy = gy0; gy <= gy1; ++gy) {
    for (int64_t gx = gx0; gx <= gx1; ++gx) {
      const Rect overlap = intersection(clipped, gridRect(gx, gy));
      if (!isValidRect(overlap)) continue;
      const int64_t area = gridArea(gx, gy);
      if (area <= 0) continue;

      Grid grid;
      const auto it = grids_.find(key(gx, gy));
      if (it != grids_.end()) grid = it->second;
      if (grid.macro_area >= area) continue;

      const double available =
          static_cast<double>(std::max<int64_t>(1, area - grid.macro_area));
      const double added = static_cast<double>(rectArea(overlap));
      const double density =
          100.0 * (static_cast<double>(grid.movable_area) + added) / available;
      const double overflow = std::max(0.0, density - threshold_);
      const double weight = added / static_cast<double>(area);
      weighted_overflow += overflow * weight;
      touched_weight += weight;
    }
  }
  if (touched_weight <= 0.0) return 0.0;
  return weighted_overflow / touched_weight;
}

void DensityEstimator::commit(const Rect& rect) {
  addArea(intersection(rect, design_.die), false);
}

void DensityEstimator::rebuildMovableOccupancy() {
  for (auto& entry : grids_) {
    entry.second.movable_area = 0;
  }
  for (const Cell& cell : design_.cells) {
    if (cell.has_placement) commit(cell.placed);
  }
}

}  // namespace legalizer
