#include "legalizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace legalizer {
namespace {

constexpr double kNormFactor = 18.2;
constexpr double kTailRepairMinGainUm = 5.0;
constexpr double kRepairEps = 1e-9;
constexpr size_t kRepairCandidateLimit = 128;

int constrainednessBucket(int64_t starts) {
  if (starts <= 1) return 0;
  if (starts <= 8) return 1;
  if (starts <= 64) return 2;
  if (starts <= 512) return 3;
  if (starts <= 4096) return 4;
  return 5;
}

void subtractSegment(std::vector<RowInterval>& intervals, int64_t cut_min,
                     int64_t cut_max) {
  std::vector<RowInterval> next;
  for (const RowInterval& interval : intervals) {
    if (cut_max <= interval.x_min || interval.x_max <= cut_min) {
      next.push_back(interval);
      continue;
    }
    if (interval.x_min < cut_min) {
      next.push_back(RowInterval{interval.x_min, std::min(cut_min, interval.x_max)});
    }
    if (cut_max < interval.x_max) {
      next.push_back(RowInterval{std::max(cut_max, interval.x_min), interval.x_max});
    }
  }
  intervals = std::move(next);
}

std::vector<RowInterval> intersectIntervalSets(
    const std::vector<RowInterval>& a, const std::vector<RowInterval>& b) {
  std::vector<RowInterval> out;
  size_t i = 0;
  size_t j = 0;
  while (i < a.size() && j < b.size()) {
    const int64_t lo = std::max(a[i].x_min, b[j].x_min);
    const int64_t hi = std::min(a[i].x_max, b[j].x_max);
    if (lo < hi) out.push_back(RowInterval{lo, hi});
    if (a[i].x_max < b[j].x_max) {
      ++i;
    } else {
      ++j;
    }
  }
  return out;
}

}  // namespace

Legalizer::Legalizer(Design& design, const std::vector<LegalRow>& rows,
                     DensityEstimator& density, double alpha)
    : design_(design), rows_(rows), density_(density), alpha_(alpha) {
  occupancy_.resize(rows_.size());
}

std::vector<size_t> Legalizer::placementOrder() const {
  std::vector<size_t> order(design_.cells.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;

  std::vector<int64_t> constrained_starts(design_.cells.size(), 0);
  for (size_t i = 0; i < design_.cells.size(); ++i) {
    constrained_starts[i] = constrainedStartCount(design_.cells[i]);
  }

  std::stable_sort(order.begin(), order.end(), [this, &constrained_starts](
                                             size_t a, size_t b) {
    const Cell& ca = design_.cells[a];
    const Cell& cb = design_.cells[b];
    const int bucket_a = constrainednessBucket(constrained_starts[a]);
    const int bucket_b = constrainednessBucket(constrained_starts[b]);
    if (bucket_a != bucket_b) return bucket_a < bucket_b;
    const int64_t area_a = rectArea(ca.original);
    const int64_t area_b = rectArea(cb.original);
    if (area_a != area_b) return area_a > area_b;
    const int64_t h_a = rectHeight(ca.original);
    const int64_t h_b = rectHeight(cb.original);
    if (h_a != h_b) return h_a > h_b;
    if (ca.original.y_min != cb.original.y_min)
      return ca.original.y_min < cb.original.y_min;
    if (ca.original.x_min != cb.original.x_min)
      return ca.original.x_min < cb.original.x_min;
    return ca.input_index < cb.input_index;
  });
  return order;
}

int64_t Legalizer::constrainedStartCount(const Cell& cell) const {
  const int64_t width = rectWidth(cell.original);
  const int64_t height = rectHeight(cell.original);
  const int64_t row_span = height / design_.site_height;
  if (row_span <= 0 || static_cast<size_t>(row_span) > rows_.size()) return 0;

  const int64_t preferred_row =
      clampInt64(floorDiv(cell.original.y_min - design_.die.y_min,
                          design_.site_height),
                 0, static_cast<int64_t>(rows_.size()) - row_span);
  const int64_t row_radius = std::min<int64_t>(16, rows_.size());
  const int64_t first = std::max<int64_t>(0, preferred_row - row_radius);
  const int64_t last = std::min<int64_t>(
      static_cast<int64_t>(rows_.size()) - row_span, preferred_row + row_radius);

  int64_t starts = 0;
  for (int64_t row = first; row <= last; ++row) {
    const std::vector<RowInterval> intervals =
        commonFreeIntervals(static_cast<size_t>(row), row_span);
    for (const RowInterval& interval : intervals) {
      const int64_t start_min =
          snapUp(interval.x_min, design_.die.x_min, design_.site_width);
      const int64_t start_max =
          snapDown(interval.x_max - width, design_.die.x_min, design_.site_width);
      if (start_min <= start_max) {
        starts += 1 + (start_max - start_min) / design_.site_width;
      }
    }
    if (starts > 1000000000LL) return starts;
  }
  return starts;
}

std::vector<size_t> Legalizer::rowOrderForCell(const Cell& cell) const {
  std::vector<size_t> order;
  const int64_t row_span = rectHeight(cell.original) / design_.site_height;
  if (row_span <= 0 || static_cast<size_t>(row_span) > rows_.size()) {
    return order;
  }
  const size_t max_start = rows_.size() - static_cast<size_t>(row_span);
  order.reserve(max_start + 1);
  for (size_t i = 0; i <= max_start; ++i) order.push_back(i);

  std::stable_sort(order.begin(), order.end(), [this, &cell](size_t a, size_t b) {
    const int64_t da = std::llabs(rows_[a].y - cell.original.y_min);
    const int64_t db = std::llabs(rows_[b].y - cell.original.y_min);
    if (da != db) return da < db;
    return rows_[a].y < rows_[b].y;
  });
  return order;
}

std::vector<RowInterval> Legalizer::commonFreeIntervals(size_t start_row,
                                                        int64_t row_span) const {
  std::vector<RowInterval> common = rows_[start_row].free_intervals;
  for (int64_t offset = 1; offset < row_span && !common.empty(); ++offset) {
    common = intersectIntervalSets(
        common, rows_[start_row + static_cast<size_t>(offset)].free_intervals);
  }
  return common;
}

std::vector<RowInterval> Legalizer::subtractOccupancy(
    std::vector<RowInterval> intervals, size_t start_row, int64_t row_span) const {
  for (int64_t offset = 0; offset < row_span && !intervals.empty(); ++offset) {
    const auto& segments = occupancy_[start_row + static_cast<size_t>(offset)];
    for (const Segment& segment : segments) {
      subtractSegment(intervals, segment.x_min, segment.x_max);
      if (intervals.empty()) break;
    }
  }
  return intervals;
}

bool Legalizer::segmentOverlaps(const std::vector<Segment>& segments,
                                int64_t x_min, int64_t x_max) const {
  const auto it = std::lower_bound(
      segments.begin(), segments.end(), x_min,
      [](const Segment& segment, int64_t value) { return segment.x_max <= value; });
  return it != segments.end() && it->x_min < x_max;
}

bool Legalizer::isBetterCandidate(const Candidate& cand,
                                  const Candidate& best) const {
  constexpr double eps = 1e-9;
  if (cand.total_cost + eps < best.total_cost) return true;
  if (std::abs(cand.total_cost - best.total_cost) > eps) return false;
  if (cand.displacement_cost + eps < best.displacement_cost) return true;
  if (std::abs(cand.displacement_cost - best.displacement_cost) > eps)
    return false;
  if (cand.y != best.y) return cand.y < best.y;
  return cand.x < best.x;
}

bool Legalizer::evaluateCandidatesForRow(size_t cell_index, size_t start_row,
                                         Candidate& best,
                                         bool& has_best,
                                         bool displacement_only) const {
  const Cell& cell = design_.cells[cell_index];
  const int64_t width = rectWidth(cell.original);
  const int64_t height = rectHeight(cell.original);
  const int64_t row_span = height / design_.site_height;

  std::vector<RowInterval> intervals =
      subtractOccupancy(commonFreeIntervals(start_row, row_span), start_row,
                        row_span);
  if (intervals.empty()) return false;

  bool found = false;
  for (const RowInterval& interval : intervals) {
    const int64_t start_min =
        snapUp(interval.x_min, design_.die.x_min, design_.site_width);
    const int64_t start_max =
        snapDown(interval.x_max - width, design_.die.x_min, design_.site_width);
    if (start_min > start_max) continue;

    const int64_t preferred = clampInt64(cell.original.x_min, start_min, start_max);
    const int64_t snapped_pref =
        clampInt64(snapDown(preferred, design_.die.x_min, design_.site_width),
                   start_min, start_max);

    std::vector<int64_t> xs;
    const int64_t site_count = 1 + (start_max - start_min) / design_.site_width;
    const int64_t width_sites = std::max<int64_t>(
        1, ceilDiv(width, design_.site_width));
    const int64_t step_budget =
        std::min<int64_t>(site_count - 1, std::max<int64_t>(8, width_sites + 16));
    xs.reserve(static_cast<size_t>(6 + 2 * step_budget));
    xs.push_back(snapped_pref);
    xs.push_back(clampInt64(snapUp(preferred, design_.die.x_min,
                                   design_.site_width),
                            start_min, start_max));
    xs.push_back(start_min);
    xs.push_back(start_max);
    xs.push_back(clampInt64(start_min + width, start_min, start_max));
    xs.push_back(clampInt64(start_max - width, start_min, start_max));
    for (int64_t step = 1; step <= step_budget; ++step) {
      const int64_t delta = step * design_.site_width;
      if (snapped_pref - delta >= start_min) xs.push_back(snapped_pref - delta);
      if (snapped_pref + delta <= start_max) xs.push_back(snapped_pref + delta);
    }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());

    for (int64_t x : xs) {
      const int64_t y = rows_[start_row].y;
      const Rect placed = makeRect(x, y, width, height);
      if (!contains(design_.die, placed)) continue;
      bool overlap = false;
      for (int64_t offset = 0; offset < row_span; ++offset) {
        if (segmentOverlaps(occupancy_[start_row + static_cast<size_t>(offset)],
                            x, x + width)) {
          overlap = true;
          break;
        }
      }
      if (overlap) continue;

      const double disp_um = static_cast<double>(
                                 manhattanDisplacement(cell.original, placed)) /
                             static_cast<double>(design_.dbu_per_micron);
      const double disp_cost = disp_um * kNormFactor;
      const double density_cost =
          displacement_only ? 0.0 : density_.scoreCandidate(placed);
      const double density_weight = displacement_only ? 0.0 : (1.0 - alpha_);
      Candidate cand{start_row, x, y,
                     displacement_only
                         ? disp_cost
                         : alpha_ * disp_cost + density_weight * density_cost,
                     disp_cost, density_cost};
      if (!has_best || isBetterCandidate(cand, best)) {
        best = cand;
        has_best = true;
      }
      found = true;
    }
  }

  return found;
}

void Legalizer::commit(size_t cell_index, const Candidate& candidate) {
  Cell& cell = design_.cells[cell_index];
  cell.placed = makeRect(candidate.x, candidate.y, rectWidth(cell.original),
                         rectHeight(cell.original));
  cell.has_placement = true;

  const int64_t row_span = rectHeight(cell.original) / design_.site_height;
  for (int64_t offset = 0; offset < row_span; ++offset) {
    auto& segments = occupancy_[candidate.row_index + static_cast<size_t>(offset)];
    segments.push_back(Segment{cell.placed.x_min, cell.placed.x_max});
    std::sort(segments.begin(), segments.end(), [](const Segment& a,
                                                   const Segment& b) {
      if (a.x_min != b.x_min) return a.x_min < b.x_min;
      return a.x_max < b.x_max;
    });
  }
  density_.commit(cell.placed);
}

void Legalizer::removeFromOccupancy(size_t cell_index) {
  const Cell& cell = design_.cells[cell_index];
  if (!cell.has_placement) return;
  const int64_t start_row = rowIndexForY(cell.placed.y_min);
  const int64_t row_span = rectHeight(cell.original) / design_.site_height;
  if (start_row < 0) return;

  for (int64_t offset = 0; offset < row_span; ++offset) {
    const size_t row = static_cast<size_t>(start_row + offset);
    if (row >= occupancy_.size()) continue;
    auto& segments = occupancy_[row];
    const auto it = std::find_if(
        segments.begin(), segments.end(), [&cell](const Segment& segment) {
          return segment.x_min == cell.placed.x_min &&
                 segment.x_max == cell.placed.x_max;
        });
    if (it != segments.end()) segments.erase(it);
  }
}

bool Legalizer::findBestCandidate(size_t cell_index, size_t row_budget,
                                  bool displacement_only,
                                  Candidate& best) const {
  bool has_best = false;
  const std::vector<size_t> row_order = rowOrderForCell(design_.cells[cell_index]);
  size_t feasible_rows_seen = 0;
  for (size_t row_index : row_order) {
    if (evaluateCandidatesForRow(cell_index, row_index, best, has_best,
                                 displacement_only)) {
      ++feasible_rows_seen;
      if (has_best && feasible_rows_seen >= row_budget) break;
    }
  }
  return has_best;
}

bool Legalizer::findBestRepairCandidate(size_t cell_index, double old_disp,
                                        double old_total_disp,
                                        double old_max_disp,
                                        double old_density_proxy,
                                        Candidate& best) const {
  std::vector<Candidate> candidates;
  const Cell& cell = design_.cells[cell_index];
  const int64_t width = rectWidth(cell.original);
  const int64_t height = rectHeight(cell.original);
  const int64_t row_span = height / design_.site_height;

  auto better_repair = [](const Candidate& a, const Candidate& b) {
    constexpr double eps = 1e-9;
    if (std::abs(a.repair_max_displacement - b.repair_max_displacement) > eps) {
      return a.repair_max_displacement < b.repair_max_displacement;
    }
    if (std::abs(a.density_cost - b.density_cost) > eps) {
      return a.density_cost < b.density_cost;
    }
    if (std::abs(a.repair_total_displacement - b.repair_total_displacement) >
        eps) {
      return a.repair_total_displacement < b.repair_total_displacement;
    }
    if (std::abs(a.displacement_cost - b.displacement_cost) > eps) {
      return a.displacement_cost < b.displacement_cost;
    }
    if (a.y != b.y) return a.y < b.y;
    return a.x < b.x;
  };

  auto remember_candidate = [&](Candidate cand) {
    candidates.push_back(cand);
    std::sort(candidates.begin(), candidates.end(), better_repair);
    if (candidates.size() > kRepairCandidateLimit) candidates.pop_back();
  };

  const std::vector<size_t> row_order = rowOrderForCell(cell);
  for (size_t start_row : row_order) {
    std::vector<RowInterval> intervals =
        subtractOccupancy(commonFreeIntervals(start_row, row_span), start_row,
                          row_span);
    if (intervals.empty()) continue;

    for (const RowInterval& interval : intervals) {
      const int64_t start_min =
          snapUp(interval.x_min, design_.die.x_min, design_.site_width);
      const int64_t start_max =
          snapDown(interval.x_max - width, design_.die.x_min, design_.site_width);
      if (start_min > start_max) continue;

      const int64_t preferred =
          clampInt64(cell.original.x_min, start_min, start_max);
      const int64_t snapped_pref =
          clampInt64(snapDown(preferred, design_.die.x_min, design_.site_width),
                     start_min, start_max);

      std::vector<int64_t> xs;
      const int64_t site_count = 1 + (start_max - start_min) / design_.site_width;
      const int64_t width_sites =
          std::max<int64_t>(1, ceilDiv(width, design_.site_width));
      const int64_t step_budget = std::min<int64_t>(
          site_count - 1, std::max<int64_t>(8, width_sites + 16));
      xs.reserve(static_cast<size_t>(6 + 2 * step_budget));
      xs.push_back(snapped_pref);
      xs.push_back(clampInt64(snapUp(preferred, design_.die.x_min,
                                     design_.site_width),
                              start_min, start_max));
      xs.push_back(start_min);
      xs.push_back(start_max);
      xs.push_back(clampInt64(start_min + width, start_min, start_max));
      xs.push_back(clampInt64(start_max - width, start_min, start_max));
      for (int64_t step = 1; step <= step_budget; ++step) {
        const int64_t delta = step * design_.site_width;
        if (snapped_pref - delta >= start_min) xs.push_back(snapped_pref - delta);
        if (snapped_pref + delta <= start_max) xs.push_back(snapped_pref + delta);
      }
      std::sort(xs.begin(), xs.end());
      xs.erase(std::unique(xs.begin(), xs.end()), xs.end());

      for (int64_t x : xs) {
        const int64_t y = rows_[start_row].y;
        const Rect placed = makeRect(x, y, width, height);
        if (!contains(design_.die, placed)) continue;
        bool overlap = false;
        for (int64_t offset = 0; offset < row_span; ++offset) {
          if (segmentOverlaps(occupancy_[start_row + static_cast<size_t>(offset)],
                              x, x + width)) {
            overlap = true;
            break;
          }
        }
        if (overlap) continue;

        const double new_disp = displacementUmWithPlacement(cell, placed);
        const double new_total_disp = old_total_disp - old_disp + new_disp;
        if (new_total_disp > old_total_disp + kRepairEps) continue;

        const double new_max_disp =
            maxDisplacementUmWithPlacement(cell_index, placed);
        if (new_max_disp >
            old_max_disp - kTailRepairMinGainUm + kRepairEps) {
          continue;
        }

        const double disp_cost = new_disp * kNormFactor;
        const double density_cost = density_.scoreCandidate(placed);
        Candidate cand{start_row,
                       x,
                       y,
                       alpha_ * disp_cost + (1.0 - alpha_) * density_cost,
                       disp_cost,
                       density_cost,
                       new_total_disp,
                       new_max_disp};
        remember_candidate(cand);
      }
    }
  }

  if (candidates.empty()) return false;
  for (const Candidate& cand : candidates) {
    const Rect placed = makeRect(cand.x, cand.y, width, height);
    const double new_density_proxy =
        density_.overflowProxyWithCandidate(placed);
    if (new_density_proxy <= old_density_proxy + kRepairEps) {
      best = cand;
      return true;
    }
  }
  return false;
}

double Legalizer::displacementUm(const Cell& cell) const {
  return displacementUmWithPlacement(cell, cell.placed);
}

double Legalizer::displacementUmWithPlacement(const Cell& cell,
                                             const Rect& placed) const {
  return static_cast<double>(manhattanDisplacement(cell.original, placed)) /
         static_cast<double>(design_.dbu_per_micron);
}

double Legalizer::totalDisplacementUm() const {
  double total = 0.0;
  for (const Cell& cell : design_.cells) {
    if (cell.has_placement) total += displacementUm(cell);
  }
  return total;
}

double Legalizer::maxDisplacementUm() const {
  double max_disp = 0.0;
  for (const Cell& cell : design_.cells) {
    if (cell.has_placement) max_disp = std::max(max_disp, displacementUm(cell));
  }
  return max_disp;
}

double Legalizer::maxDisplacementUmWithPlacement(size_t cell_index,
                                                 const Rect& placed) const {
  double max_disp = 0.0;
  for (size_t i = 0; i < design_.cells.size(); ++i) {
    const Cell& cell = design_.cells[i];
    if (i == cell_index) {
      max_disp = std::max(max_disp, displacementUmWithPlacement(cell, placed));
    } else if (cell.has_placement) {
      max_disp = std::max(max_disp, displacementUm(cell));
    }
  }
  return max_disp;
}

int64_t Legalizer::rowIndexForY(int64_t y) const {
  if (y < design_.die.y_min || design_.site_height <= 0) return -1;
  const int64_t row = (y - design_.die.y_min) / design_.site_height;
  if (row < 0 || static_cast<size_t>(row) >= rows_.size()) return -1;
  if (rows_[static_cast<size_t>(row)].y != y) return -1;
  return row;
}

void Legalizer::repairDisplacementTail() {
  if (design_.cells.size() < 2) return;

  std::vector<size_t> order(design_.cells.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [this](size_t a, size_t b) {
    const double da = displacementUm(design_.cells[a]);
    const double db = displacementUm(design_.cells[b]);
    constexpr double eps = 1e-9;
    if (std::abs(da - db) > eps) return da > db;
    return design_.cells[a].input_index < design_.cells[b].input_index;
  });

  const size_t attempts = std::min<size_t>(order.size(), 64);
  auto restore_old = [this](size_t cell_index, const Rect& old_placed) {
    Cell& cell = design_.cells[cell_index];
    cell.placed = old_placed;
    cell.has_placement = false;
    const int64_t row = rowIndexForY(old_placed.y_min);
    if (row >= 0) {
      Candidate old_candidate{static_cast<size_t>(row), old_placed.x_min,
                              old_placed.y_min, 0.0, 0.0, 0.0};
      commit(cell_index, old_candidate);
    }
  };

  for (size_t pos = 0; pos < attempts; ++pos) {
    const size_t cell_index = order[pos];
    Cell& cell = design_.cells[cell_index];
    if (!cell.has_placement) continue;
    const Rect old_placed = cell.placed;
    const double old_disp = displacementUm(cell);
    const double old_total_disp = totalDisplacementUm();
    const double old_max_disp = maxDisplacementUm();
    const double old_density_proxy = density_.overflowProxy();

    removeFromOccupancy(cell_index);
    cell.has_placement = false;
    density_.rebuildMovableOccupancy();

    Candidate best;
    if (!findBestRepairCandidate(cell_index, old_disp, old_total_disp,
                                 old_max_disp, old_density_proxy, best)) {
      restore_old(cell_index, old_placed);
      continue;
    }

    commit(cell_index, best);
  }

  density_.rebuildMovableOccupancy();
}

bool Legalizer::legalize(std::string& error) {
  if (rows_.empty()) {
    error = "no legal rows available";
    return false;
  }

  for (size_t cell_index : placementOrder()) {
    const Cell& cell = design_.cells[cell_index];
    const int64_t height = rectHeight(cell.original);
    if (height % design_.site_height != 0) {
      error = "unsupported non-row-multiple cell height: " + cell.name;
      return false;
    }
    const int64_t row_span = height / design_.site_height;
    if (row_span <= 0 || static_cast<size_t>(row_span) > rows_.size()) {
      error = "cell cannot fit in available rows: " + cell.name;
      return false;
    }

    const size_t row_budget = (alpha_ < 0.20) ? 96 : ((alpha_ < 0.60) ? 64 : 32);
    Candidate best;
    const bool has_best = findBestCandidate(cell_index, row_budget, false, best);

    if (!has_best) {
      error = "cannot place cell " + cell.name + " (" +
              std::to_string(rectWidth(cell.original)) + "x" +
              std::to_string(rectHeight(cell.original)) + " DBU)";
      return false;
    }

    commit(cell_index, best);
  }

  repairDisplacementTail();

  return true;
}

}  // namespace legalizer
