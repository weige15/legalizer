#include "legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <tuple>

namespace legalizer {
namespace {

struct Candidate {
  bool valid = false;
  int rowIndex = 0;
  int intervalIndex = 0;
  std::vector<int> order;
  std::vector<Dbu> xs;
  long double score = 0.0;
};

struct Cluster {
  int start = 0;
  int end = 0;
  long double targetSum = 0.0;
  int weight = 0;

  long double target() const {
    return weight > 0 ? targetSum / static_cast<long double>(weight) : 0.0;
  }
};

Dbu totalWidth(const PlacementModel &model, const std::vector<int> &ids) {
  Dbu width = 0;
  for (int id : ids) {
    width += model.cells.at(static_cast<std::size_t>(id)).width;
  }
  return width;
}

std::vector<int> insertionOrderFor(const PlacementModel &model,
                                   const RowInterval &interval, int cellId) {
  std::vector<int> order = interval.cellIds;
  order.push_back(cellId);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    const Cell &ca = model.cells.at(static_cast<std::size_t>(a));
    const Cell &cb = model.cells.at(static_cast<std::size_t>(b));
    if (ca.original.x != cb.original.x) return ca.original.x < cb.original.x;
    if (ca.original.y != cb.original.y) return ca.original.y < cb.original.y;
    return ca.name < cb.name;
  });
  return order;
}

long double placementCost(const PlacementModel &model, const RowInterval &interval,
                          const std::vector<int> &order, const std::vector<Dbu> &xs) {
  long double cost = 0.0;
  for (std::size_t i = 0; i < order.size(); ++i) {
    const Cell &cell = model.cells.at(static_cast<std::size_t>(order[i]));
    cost += std::llabs(xs[i] - cell.original.x);
    cost += std::llabs(interval.y - cell.original.y);
  }
  return cost;
}

long double currentIntervalCost(const PlacementModel &model,
                                const RowInterval &interval) {
  std::vector<Dbu> xs;
  xs.reserve(interval.cellIds.size());
  for (int cellId : interval.cellIds) {
    xs.push_back(model.cells.at(static_cast<std::size_t>(cellId)).placed.x);
  }
  return placementCost(model, interval, interval.cellIds, xs);
}

long double intersectionArea(const Rect &a, const Rect &b) {
  Rect r{std::max(a.lx, b.lx), std::max(a.ly, b.ly),
         std::min(a.ux, b.ux), std::min(a.uy, b.uy)};
  return static_cast<long double>(rectArea(r));
}

void commitInterval(PlacementModel *model, RowInterval *interval,
                    const std::vector<int> &order, const std::vector<Dbu> &xs) {
  interval->cellIds = order;
  interval->occupiedWidth = 0;
  for (std::size_t i = 0; i < order.size(); ++i) {
    Cell &cell = model->cells.at(static_cast<std::size_t>(order[i]));
    cell.placed = Point{xs[i], interval->y};
    cell.placedValid = true;
    interval->occupiedWidth += cell.width;
  }
}

std::vector<int> cellProcessingOrder(const PlacementModel &model, bool reverse) {
  std::vector<int> order(model.cells.size());
  for (std::size_t i = 0; i < model.cells.size(); ++i) {
    order[i] = static_cast<int>(i);
  }
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    const Cell &ca = model.cells.at(static_cast<std::size_t>(a));
    const Cell &cb = model.cells.at(static_cast<std::size_t>(b));
    if (ca.original.x != cb.original.x) return ca.original.x < cb.original.x;
    if (ca.original.y != cb.original.y) return ca.original.y < cb.original.y;
    return ca.name < cb.name;
  });
  if (reverse) {
    std::reverse(order.begin(), order.end());
  }
  return order;
}

std::vector<int> rowSearchOrder(const Tech &tech, Dbu preferredY) {
  int rows = rowCount(tech);
  int nearest = nearestRowIndexForY(tech, preferredY);
  std::vector<int> order(rows);
  for (int i = 0; i < rows; ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    Dbu da = std::llabs(rowY(tech, a) - preferredY);
    Dbu db = std::llabs(rowY(tech, b) - preferredY);
    if (da != db) return da < db;
    return std::llabs(a - nearest) < std::llabs(b - nearest) ||
           (std::llabs(a - nearest) == std::llabs(b - nearest) && a < b);
  });
  return order;
}

struct GapChoice {
  bool valid = false;
  int rowIndex = 0;
  int intervalIndex = 0;
  Dbu x = 0;
  long double score = 0.0;
};

bool betterGap(const GapChoice &a, const GapChoice &b) {
  if (!b.valid) return true;
  if (a.score != b.score) return a.score < b.score;
  if (a.rowIndex != b.rowIndex) return a.rowIndex < b.rowIndex;
  if (a.intervalIndex != b.intervalIndex) return a.intervalIndex < b.intervalIndex;
  return a.x < b.x;
}

Dbu clampClusterOrigin(const Tech &tech, long double target, Dbu minX, Dbu maxX) {
  if (maxX < minX) {
    return minX;
  }
  long double rounded = std::round(target);
  if (rounded < static_cast<long double>(std::numeric_limits<Dbu>::min())) {
    return snapUpToSite(tech, minX);
  }
  if (rounded > static_cast<long double>(std::numeric_limits<Dbu>::max())) {
    return snapDownToSite(tech, maxX);
  }
  return clampSiteOrigin(tech, static_cast<Dbu>(rounded), minX, maxX);
}

}  // namespace

namespace {

Status legalizePlacementWithOrder(PlacementModel *model, std::vector<Row> *rows,
                                  bool reverseOrder) {
  for (int cellId : cellProcessingOrder(*model, reverseOrder)) {
    const Cell &cell = model->cells.at(static_cast<std::size_t>(cellId));
    Candidate best;
    int inspectedRows = 0;
    for (int rowIdx : rowSearchOrder(model->tech, cell.original.y)) {
      if (rowIdx < 0 || rowIdx >= static_cast<int>(rows->size())) {
        continue;
      }
      Dbu verticalCost = std::llabs(rowY(model->tech, rowIdx) - cell.original.y);
      if (best.valid && static_cast<long double>(verticalCost) > best.score) {
        break;
      }
      Row &row = rows->at(static_cast<std::size_t>(rowIdx));
      ++inspectedRows;
      bool rowHasCapacity = false;
      for (RowInterval &interval : row.intervals) {
        if (interval.occupiedWidth + cell.width > interval.xMax - interval.xMin) {
          continue;
        }
        rowHasCapacity = true;
        std::vector<int> order = insertionOrderFor(*model, interval, cellId);
        IntervalSolveResult solved = solveIntervalAbacus(*model, interval, order);
        if (!solved.ok) {
          continue;
        }
        long double score = solved.cost - currentIntervalCost(*model, interval);
        bool better = !best.valid || score < best.score ||
                      (score == best.score &&
                       std::tie(rowIdx, interval.intervalIndex) <
                           std::tie(best.rowIndex, best.intervalIndex));
        if (better) {
          best.valid = true;
          best.rowIndex = rowIdx;
          best.intervalIndex = interval.intervalIndex;
          best.order = order;
          best.xs = solved.xByOrder;
          best.score = score;
        }
      }
      if (best.valid && rowHasCapacity && inspectedRows >= 12) {
        break;
      }
    }
    if (best.valid) {
      RowInterval &interval =
          rows->at(static_cast<std::size_t>(best.rowIndex))
              .intervals.at(static_cast<std::size_t>(best.intervalIndex));
      commitInterval(model, &interval, best.order, best.xs);
      continue;
    }

    Status fallback = tetrisPlaceCell(model, rows, cellId);
    if (!fallback.ok) {
      return Status::Error("failed to legalize CELL '" + cell.name + "': " +
                           fallback.message);
    }
  }
  return Status::Ok();
}

}  // namespace

IntervalSolveResult solveIntervalAbacus(const PlacementModel &model,
                                         const RowInterval &interval,
                                         const std::vector<int> &orderedCellIds) {
  IntervalSolveResult result;
  result.ok = true;
  result.xByOrder.resize(orderedCellIds.size());
  if (orderedCellIds.empty()) {
    result.cost = 0.0;
    return result;
  }

  if (totalWidth(model, orderedCellIds) > interval.xMax - interval.xMin) {
    return IntervalSolveResult{false, "cell width exceeds interval capacity", {}, 0.0};
  }

  std::vector<Dbu> prefix(orderedCellIds.size(), 0);
  Dbu runningWidth = 0;
  for (std::size_t i = 0; i < orderedCellIds.size(); ++i) {
    prefix[i] = runningWidth;
    runningWidth +=
        model.cells.at(static_cast<std::size_t>(orderedCellIds[i])).width;
  }

  const Dbu minClusterX = interval.xMin;
  const Dbu maxClusterX = interval.xMax - runningWidth;
  if (maxClusterX < minClusterX) {
    return IntervalSolveResult{false, "cell width exceeds interval capacity", {}, 0.0};
  }

  std::vector<Cluster> clusters;
  clusters.reserve(orderedCellIds.size());
  for (std::size_t i = 0; i < orderedCellIds.size(); ++i) {
    const Cell &cell = model.cells.at(static_cast<std::size_t>(orderedCellIds[i]));
    long double target = static_cast<long double>(cell.original.x) -
                         static_cast<long double>(prefix[i]);
    clusters.push_back(Cluster{static_cast<int>(i), static_cast<int>(i), target, 1});
    while (clusters.size() >= 2 &&
           clusters[clusters.size() - 2].target() > clusters.back().target()) {
      Cluster merged = clusters[clusters.size() - 2];
      const Cluster &last = clusters.back();
      merged.end = last.end;
      merged.targetSum += last.targetSum;
      merged.weight += last.weight;
      clusters.pop_back();
      clusters.back() = merged;
    }
  }

  Dbu previousZ = minClusterX;
  for (const Cluster &cluster : clusters) {
    Dbu z = clampClusterOrigin(model.tech, cluster.target(), minClusterX, maxClusterX);
    if (z < previousZ) {
      z = snapUpToSite(model.tech, previousZ);
    }
    if (z > maxClusterX) {
      z = snapDownToSite(model.tech, maxClusterX);
    }
    if (z < previousZ || z < minClusterX || z > maxClusterX ||
        !isSiteAligned(model.tech, z)) {
      return IntervalSolveResult{false, "cluster snapping failed inside interval", {}, 0.0};
    }
    for (int i = cluster.start; i <= cluster.end; ++i) {
      result.xByOrder[static_cast<std::size_t>(i)] =
          z + prefix[static_cast<std::size_t>(i)];
    }
    previousZ = z;
  }

  for (std::size_t i = 0; i < orderedCellIds.size(); ++i) {
    const Cell &cell = model.cells.at(static_cast<std::size_t>(orderedCellIds[i]));
    Dbu x = result.xByOrder[i];
    if (!isSiteAligned(model.tech, x)) {
      return IntervalSolveResult{false, "site snapping failed inside interval", {}, 0.0};
    }
    if (x < interval.xMin || x + cell.width > interval.xMax) {
      return IntervalSolveResult{false, "expanded cells overflow interval", {}, 0.0};
    }
    if (i > 0) {
      const Cell &prev =
          model.cells.at(static_cast<std::size_t>(orderedCellIds[i - 1]));
      if (result.xByOrder[i - 1] + prev.width > x) {
        return IntervalSolveResult{false, "expanded cells overlap", {}, 0.0};
      }
    }
  }
  result.cost = placementCost(model, interval, orderedCellIds, result.xByOrder);
  return result;
}

Status legalizePlacement(PlacementModel *model, std::vector<Row> *rows) {
  return legalizePlacementWithOrder(model, rows, false);
}

Status legalizePlacementReverse(PlacementModel *model, std::vector<Row> *rows) {
  return legalizePlacementWithOrder(model, rows, true);
}

Status tetrisPlaceCell(PlacementModel *model, std::vector<Row> *rows, int cellId) {
  const Cell &cell = model->cells.at(static_cast<std::size_t>(cellId));
  GapChoice best;
  for (const int rowIdx : rowSearchOrder(model->tech, cell.original.y)) {
    Row &row = rows->at(static_cast<std::size_t>(rowIdx));
    for (RowInterval &interval : row.intervals) {
      if (interval.xMax - interval.xMin - interval.occupiedWidth < cell.width) {
        continue;
      }
      std::vector<std::pair<Dbu, Dbu>> spans;
      for (int otherId : interval.cellIds) {
        const Cell &other = model->cells.at(static_cast<std::size_t>(otherId));
        spans.push_back({other.placed.x, other.placed.x + other.width});
      }
      std::sort(spans.begin(), spans.end());
      Dbu cursor = interval.xMin;
      for (std::size_t i = 0; i <= spans.size(); ++i) {
        Dbu gapEnd = (i < spans.size()) ? spans[i].first : interval.xMax;
        if (gapEnd - cursor >= cell.width) {
          Dbu minX = cursor;
          Dbu maxX = gapEnd - cell.width;
          Dbu x = clampSiteOrigin(model->tech, cell.original.x, minX, maxX);
          if (x >= minX && x <= maxX && isSiteAligned(model->tech, x)) {
            long double score = std::llabs(x - cell.original.x) +
                                std::llabs(interval.y - cell.original.y);
            GapChoice choice{true, rowIdx, interval.intervalIndex, x, score};
            if (betterGap(choice, best)) {
              best = choice;
            }
          }
        }
        if (i < spans.size()) {
          cursor = std::max(cursor, spans[i].second);
        }
      }
    }
  }
  if (!best.valid) {
    return Status::Error("no legal interval gap fits width " + std::to_string(cell.width));
  }

  RowInterval &interval =
      rows->at(static_cast<std::size_t>(best.rowIndex))
          .intervals.at(static_cast<std::size_t>(best.intervalIndex));
  Cell &mutableCell = model->cells.at(static_cast<std::size_t>(cellId));
  mutableCell.placed = Point{best.x, interval.y};
  mutableCell.placedValid = true;
  interval.cellIds.push_back(cellId);
  std::sort(interval.cellIds.begin(), interval.cellIds.end(), [&](int a, int b) {
    const Cell &ca = model->cells.at(static_cast<std::size_t>(a));
    const Cell &cb = model->cells.at(static_cast<std::size_t>(b));
    if (ca.placed.x != cb.placed.x) return ca.placed.x < cb.placed.x;
    return ca.name < cb.name;
  });
  recomputeOccupiedWidth(*model, &interval);
  return Status::Ok();
}

Status runDorRepair(PlacementModel *model, std::vector<Row> *rows, double alpha,
                    double threshold) {
  std::vector<std::string> diagnostics = validateLegality(*model, *rows);
  if (!diagnostics.empty()) {
    return Status::Error("repair skipped because incumbent placement is illegal: " +
                         diagnostics.front());
  }

  Metrics current = evaluateMetrics(*model, alpha, threshold);
  const int maxPasses = 2;
  const int maxGrids = 8;
  const int maxCellsPerGrid = 8;
  const double epsilon = 1e-9;

  for (int pass = 0; pass < maxPasses; ++pass) {
    bool accepted = false;
    std::vector<DensityGridInfo> grids = computeDensityGrids(*model, threshold);
    std::sort(grids.begin(), grids.end(), [](const DensityGridInfo &a,
                                             const DensityGridInfo &b) {
      if (a.overflow != b.overflow) return a.overflow > b.overflow;
      if (a.densityPercent != b.densityPercent) return a.densityPercent > b.densityPercent;
      return a.index < b.index;
    });

    int inspectedGrids = 0;
    for (const DensityGridInfo &grid : grids) {
      if (!grid.overflow || inspectedGrids++ >= maxGrids) {
        break;
      }
      std::vector<std::pair<long double, int>> contributors;
      for (std::size_t i = 0; i < model->cells.size(); ++i) {
        const Cell &cell = model->cells[i];
        if (!cell.placedValid) {
          continue;
        }
        long double area = intersectionArea(grid.rect, rectForPlacedCell(cell));
        if (area > 0.0) {
          contributors.push_back({-area, static_cast<int>(i)});
        }
      }
      std::sort(contributors.begin(), contributors.end(),
                [&](const std::pair<long double, int> &a,
                    const std::pair<long double, int> &b) {
                  if (a.first != b.first) return a.first < b.first;
                  return model->cells.at(static_cast<std::size_t>(a.second)).name <
                         model->cells.at(static_cast<std::size_t>(b.second)).name;
                });

      int inspectedCells = 0;
      for (const auto &entry : contributors) {
        if (inspectedCells++ >= maxCellsPerGrid) {
          break;
        }
        int cellId = entry.second;
        PlacementModel trialModel = *model;
        std::vector<Row> trialRows = *rows;
        for (Row &row : trialRows) {
          for (RowInterval &interval : row.intervals) {
            auto it = std::find(interval.cellIds.begin(), interval.cellIds.end(), cellId);
            if (it != interval.cellIds.end()) {
              interval.cellIds.erase(it);
              recomputeOccupiedWidth(trialModel, &interval);
            }
          }
        }
        trialModel.cells.at(static_cast<std::size_t>(cellId)).placedValid = false;

        PlacementModel bestModel;
        std::vector<Row> bestRows;
        Metrics bestMetrics = current;
        bool foundBetter = false;
        const int maxCandidateRows = 12;
        const int maxCandidatePlacements = 12;
        int inspectedRows = 0;
        int inspectedPlacements = 0;
        const Cell &cell = trialModel.cells.at(static_cast<std::size_t>(cellId));
        for (int rowIdx : rowSearchOrder(trialModel.tech, cell.original.y)) {
          if (inspectedRows++ >= maxCandidateRows ||
              inspectedPlacements >= maxCandidatePlacements) {
            break;
          }
          if (rowIdx < 0 || rowIdx >= static_cast<int>(trialRows.size())) {
            continue;
          }
          const Row &row = trialRows.at(static_cast<std::size_t>(rowIdx));
          for (const RowInterval &interval : row.intervals) {
            if (inspectedPlacements >= maxCandidatePlacements) {
              break;
            }
            if (interval.occupiedWidth + cell.width > interval.xMax - interval.xMin) {
              continue;
            }
            ++inspectedPlacements;
            PlacementModel candidateModel = trialModel;
            std::vector<Row> candidateRows = trialRows;
            RowInterval &candidateInterval =
                candidateRows.at(static_cast<std::size_t>(rowIdx))
                    .intervals.at(static_cast<std::size_t>(interval.intervalIndex));
            std::vector<int> order =
                insertionOrderFor(candidateModel, candidateInterval, cellId);
            IntervalSolveResult solved =
                solveIntervalAbacus(candidateModel, candidateInterval, order);
            if (!solved.ok) {
              continue;
            }
            commitInterval(&candidateModel, &candidateInterval, order, solved.xByOrder);
            diagnostics = validateLegality(candidateModel, candidateRows);
            if (!diagnostics.empty()) {
              continue;
            }
            Metrics candidate = evaluateMetrics(candidateModel, alpha, threshold);
            if (candidate.quality + epsilon < bestMetrics.quality) {
              bestModel = candidateModel;
              bestRows = candidateRows;
              bestMetrics = candidate;
              foundBetter = true;
            }
          }
        }

        if (foundBetter) {
          *model = bestModel;
          *rows = bestRows;
          current = bestMetrics;
          accepted = true;
          break;
        }
      }
      if (accepted) {
        break;
      }
    }
    if (!accepted) {
      break;
    }
  }
  return Status::Ok();
}

std::vector<std::string> validateLegality(const PlacementModel &model,
                                          const std::vector<Row> &rows) {
  std::vector<std::string> errors;
  for (std::size_t i = 0; i < model.cells.size(); ++i) {
    const Cell &cell = model.cells[i];
    std::string prefix = "CELL '" + cell.name + "': ";
    if (!cell.placedValid) {
      errors.push_back(prefix + "missing placement");
      continue;
    }
    Rect rect = rectForPlacedCell(cell);
    if (!contains(model.tech.die, rect)) {
      errors.push_back(prefix + "outside die");
    }
    if (!isSiteAligned(model.tech, cell.placed.x)) {
      errors.push_back(prefix + "off site");
    }
    if (!isRowAligned(model.tech, cell.placed.y)) {
      errors.push_back(prefix + "off row");
    }
    if (!cell.originalOrient.empty() && cell.orient != cell.originalOrient) {
      errors.push_back(prefix + "orientation changed");
    }
    std::string why;
    if (!isSupportedCell(model.tech, cell, &why)) {
      errors.push_back(prefix + why);
    }
    for (const Obstacle &obs : model.obstacles) {
      if (overlaps(rect, obs.rect)) {
        errors.push_back(prefix + "overlaps obstacle '" + obs.name + "'");
      }
    }
    if (findContainingInterval(rows, cell, model.tech) == nullptr) {
      errors.push_back(prefix + "outside legal row interval");
    }
  }

  std::vector<int> ids(model.cells.size());
  for (std::size_t i = 0; i < ids.size(); ++i) {
    ids[i] = static_cast<int>(i);
  }
  std::sort(ids.begin(), ids.end(), [&](int a, int b) {
    const Cell &ca = model.cells.at(static_cast<std::size_t>(a));
    const Cell &cb = model.cells.at(static_cast<std::size_t>(b));
    if (ca.placed.y != cb.placed.y) return ca.placed.y < cb.placed.y;
    if (ca.placed.x != cb.placed.x) return ca.placed.x < cb.placed.x;
    return ca.name < cb.name;
  });
  for (std::size_t idx = 1; idx < ids.size(); ++idx) {
    const Cell &prev = model.cells.at(static_cast<std::size_t>(ids[idx - 1]));
    const Cell &cur = model.cells.at(static_cast<std::size_t>(ids[idx]));
    if (!prev.placedValid || !cur.placedValid) {
      continue;
    }
    if (overlaps(rectForPlacedCell(prev), rectForPlacedCell(cur))) {
      errors.push_back("CELL '" + prev.name + "' overlaps CELL '" + cur.name + "'");
    }
  }
  return errors;
}

}  // namespace legalizer
