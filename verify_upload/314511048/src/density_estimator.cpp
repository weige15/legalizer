#include "density_estimator.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

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

  std::map<std::pair<int, int>, long double> occupiedByGrid;
  for (const Cell &cell : model.cells) {
    if (!cell.placedValid) {
      continue;
    }
    Rect cellRect = rectForPlacedCell(cell);
    if (!overlaps(cellRect, model.tech.die)) {
      continue;
    }
    int gxMin = static_cast<int>(floorDiv(cellRect.lx - model.tech.die.lx, gridSize));
    int gyMin = static_cast<int>(floorDiv(cellRect.ly - model.tech.die.ly, gridSize));
    int gxMax = static_cast<int>(
        floorDiv(std::max(cellRect.ux - 1, model.tech.die.lx) - model.tech.die.lx,
                 gridSize));
    int gyMax = static_cast<int>(
        floorDiv(std::max(cellRect.uy - 1, model.tech.die.ly) - model.tech.die.ly,
                 gridSize));
    gxMin = std::max(gxMin, 0);
    gyMin = std::max(gyMin, 0);
    for (int gy = gyMin; gy <= gyMax; ++gy) {
      Dbu y = model.tech.die.ly + static_cast<Dbu>(gy) * gridSize;
      if (y >= model.tech.die.uy) {
        continue;
      }
      for (int gx = gxMin; gx <= gxMax; ++gx) {
        Dbu x = model.tech.die.lx + static_cast<Dbu>(gx) * gridSize;
        if (x >= model.tech.die.ux) {
          continue;
        }
        Rect grid{x, y, std::min(x + gridSize, model.tech.die.ux),
                  std::min(y + gridSize, model.tech.die.uy)};
        Dbu overlap = intersectionArea(grid, cellRect);
        if (overlap > 0) {
          occupiedByGrid[{gx, gy}] += static_cast<long double>(overlap);
        }
      }
    }
  }

  const int gridCountX = static_cast<int>(
      ceilDiv(model.tech.die.ux - model.tech.die.lx, gridSize));
  const int gridCountY = static_cast<int>(
      ceilDiv(model.tech.die.uy - model.tech.die.ly, gridSize));

  int index = 0;
  for (int gy = 0; gy < gridCountY; ++gy) {
    for (int gx = 0; gx < gridCountX; ++gx) {
      Dbu x = model.tech.die.lx + static_cast<Dbu>(gx) * gridSize;
      Dbu y = model.tech.die.ly + static_cast<Dbu>(gy) * gridSize;
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
      auto occupiedIt = occupiedByGrid.find({gx, gy});
      if (occupiedIt != occupiedByGrid.end()) {
        occupied = occupiedIt->second;
      }
      long double area = static_cast<long double>(rectArea(grid));
      double density =
          area > 0.0 ? static_cast<double>(100.0L * occupied / area) : 0.0;
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
