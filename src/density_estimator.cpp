#include "density_estimator.h"

#include <algorithm>
#include <cmath>

namespace legalizer {
namespace {

Dbu intersectionArea(const Rect &a, const Rect &b) {
  Rect r{std::max(a.lx, b.lx), std::max(a.ly, b.ly),
         std::min(a.ux, b.ux), std::min(a.uy, b.uy)};
  return rectArea(r);
}

}  // namespace

std::vector<DensityGridInfo> computeDensityGrids(const PlacementModel &model,
                                                 double threshold) {
  std::vector<DensityGridInfo> grids;
  Dbu gridSize = static_cast<Dbu>(10) * model.tech.dbuPerMicron;
  if (gridSize <= 0) {
    return grids;
  }
  int index = 0;
  for (Dbu y = model.tech.die.ly; y < model.tech.die.uy; y += gridSize) {
    for (Dbu x = model.tech.die.lx; x < model.tech.die.ux; x += gridSize) {
      Rect grid{x, y, std::min(x + gridSize, model.tech.die.ux),
                std::min(y + gridSize, model.tech.die.uy)};
      bool macroCovered = false;
      for (const Obstacle &obs : model.obstacles) {
        if (obs.type == ObjectType::Macro && overlaps(grid, obs.rect)) {
          macroCovered = true;
          break;
        }
      }
      if (macroCovered) {
        continue;
      }
      long double occupied = 0.0;
      for (const Cell &cell : model.cells) {
        if (!cell.placedValid) {
          continue;
        }
        occupied += static_cast<long double>(intersectionArea(grid, rectForPlacedCell(cell)));
      }
      long double area = static_cast<long double>(rectArea(grid));
      double density = area > 0.0 ? static_cast<double>(100.0L * occupied / area) : 0.0;
      grids.push_back(DensityGridInfo{index++, grid, density, density > threshold});
    }
  }
  return grids;
}

Metrics evaluateMetrics(const PlacementModel &model, double alpha, double threshold) {
  Metrics metrics;
  long double totalDisp = 0.0;
  int placedCount = 0;
  for (const Cell &cell : model.cells) {
    if (!cell.placedValid) {
      continue;
    }
    Dbu disp = std::llabs(cell.placed.x - cell.original.x) +
               std::llabs(cell.placed.y - cell.original.y);
    totalDisp += dbuToMicron(model.tech, disp);
    ++placedCount;
  }
  metrics.averageDisplacementMicron =
      placedCount > 0 ? static_cast<double>(totalDisp / placedCount) : 0.0;

  std::vector<DensityGridInfo> grids = computeDensityGrids(model, threshold);
  metrics.totalGrids = static_cast<int>(grids.size());
  for (const DensityGridInfo &grid : grids) {
    if (grid.overflow) {
      ++metrics.overflowGrids;
    }
  }
  metrics.dorPercent = metrics.totalGrids > 0
                           ? 100.0 * static_cast<double>(metrics.overflowGrids) /
                                 static_cast<double>(metrics.totalGrids)
                           : 0.0;
  metrics.quality = alpha * metrics.averageDisplacementMicron +
                    (1.0 - alpha) * metrics.dorPercent;
  return metrics;
}

}  // namespace legalizer
