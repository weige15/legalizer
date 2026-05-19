#include "density_estimator.h"

#include <algorithm>

namespace legalizer {
namespace {

int ceilDivPositive(Coord a, Coord b) {
  return static_cast<int>((a + b - 1) / b);
}

}  // namespace

DensityGrid::DensityGrid(const PlacementModel &model) : model_(model) {
  grid_size_ = static_cast<Coord>(10) * model_.dbu_per_micron;
  if (grid_size_ <= 0) {
    grid_size_ = 1;
  }
  cols_ = std::max(1, ceilDivPositive(width(model_.die), grid_size_));
  rows_ = std::max(1, ceilDivPositive(height(model_.die), grid_size_));
  movable_area_.assign(static_cast<std::size_t>(cols_ * rows_), 0.0);
  excluded_.assign(static_cast<std::size_t>(cols_ * rows_), false);

  for (int gy = 0; gy < rows_; ++gy) {
    for (int gx = 0; gx < cols_; ++gx) {
      Rect grid = gridRect(gx, gy);
      for (const Obstacle &obstacle : model_.obstacles) {
        if (obstacle.type == ObstacleType::Macro && overlaps(grid, obstacle.rect)) {
          excluded_[index(gx, gy)] = true;
          break;
        }
      }
    }
  }
}

void DensityGrid::clearMovableArea() {
  std::fill(movable_area_.begin(), movable_area_.end(), 0.0);
}

Rect DensityGrid::gridRect(int gx, int gy) const {
  Coord llx = model_.die.llx + static_cast<Coord>(gx) * grid_size_;
  Coord lly = model_.die.lly + static_cast<Coord>(gy) * grid_size_;
  return Rect{llx, lly, std::min(model_.die.urx, llx + grid_size_),
              std::min(model_.die.ury, lly + grid_size_)};
}

std::size_t DensityGrid::index(int gx, int gy) const {
  return static_cast<std::size_t>(gy * cols_ + gx);
}

void DensityGrid::addRect(const Rect &rect, double sign) {
  Rect clipped = intersection(rect, model_.die);
  if (!isValid(clipped)) {
    return;
  }
  int gx0 = std::max(0, static_cast<int>((clipped.llx - model_.die.llx) / grid_size_));
  int gy0 = std::max(0, static_cast<int>((clipped.lly - model_.die.lly) / grid_size_));
  int gx1 = std::min(cols_ - 1, static_cast<int>((clipped.urx - 1 - model_.die.llx) / grid_size_));
  int gy1 = std::min(rows_ - 1, static_cast<int>((clipped.ury - 1 - model_.die.lly) / grid_size_));
  for (int gy = gy0; gy <= gy1; ++gy) {
    for (int gx = gx0; gx <= gx1; ++gx) {
      Rect inter = intersection(clipped, gridRect(gx, gy));
      if (isValid(inter)) {
        movable_area_[index(gx, gy)] += sign * static_cast<double>(area(inter));
      }
    }
  }
}

DensityResult DensityGrid::compute(double threshold_percent) const {
  DensityResult result;
  for (int gy = 0; gy < rows_; ++gy) {
    for (int gx = 0; gx < cols_; ++gx) {
      std::size_t idx = index(gx, gy);
      if (excluded_[idx]) {
        continue;
      }
      Rect grid = gridRect(gx, gy);
      double grid_area = static_cast<double>(area(grid));
      if (grid_area <= 0.0) {
        continue;
      }
      ++result.total_grids;
      double density = movable_area_[idx] * 100.0 / grid_area;
      if (density > threshold_percent) {
        ++result.overflow_grids;
      }
    }
  }
  if (result.total_grids > 0) {
    result.dor = static_cast<double>(result.overflow_grids) * 100.0 /
                 static_cast<double>(result.total_grids);
  }
  return result;
}

double DensityGrid::trialPenalty(const Rect &rect, double threshold_percent) const {
  Rect clipped = intersection(rect, model_.die);
  if (!isValid(clipped)) {
    return 1e9;
  }
  int touched = 0;
  double penalty = 0.0;
  int gx0 = std::max(0, static_cast<int>((clipped.llx - model_.die.llx) / grid_size_));
  int gy0 = std::max(0, static_cast<int>((clipped.lly - model_.die.lly) / grid_size_));
  int gx1 = std::min(cols_ - 1, static_cast<int>((clipped.urx - 1 - model_.die.llx) / grid_size_));
  int gy1 = std::min(rows_ - 1, static_cast<int>((clipped.ury - 1 - model_.die.lly) / grid_size_));
  for (int gy = gy0; gy <= gy1; ++gy) {
    for (int gx = gx0; gx <= gx1; ++gx) {
      std::size_t idx = index(gx, gy);
      if (excluded_[idx]) {
        continue;
      }
      Rect grid = gridRect(gx, gy);
      Rect inter = intersection(clipped, grid);
      if (!isValid(inter)) {
        continue;
      }
      ++touched;
      double grid_area = static_cast<double>(area(grid));
      double density_after =
          (movable_area_[idx] + static_cast<double>(area(inter))) * 100.0 / grid_area;
      if (density_after > threshold_percent) {
        penalty += density_after - threshold_percent;
      }
    }
  }
  return touched == 0 ? 0.0 : penalty / static_cast<double>(touched);
}

DensityResult computeFinalDensity(const PlacementModel &model, double threshold_percent) {
  DensityGrid grid(model);
  for (const Cell &cell : model.cells) {
    if (cell.has_placement) {
      grid.addRect(cell.placed);
    }
  }
  return grid.compute(threshold_percent);
}

}  // namespace legalizer
