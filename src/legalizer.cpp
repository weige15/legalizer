#include "legalizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace legalizer {
namespace {

constexpr double kNormFactor = 18.2;

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

  std::stable_sort(order.begin(), order.end(), [this](size_t a, size_t b) {
    const Cell& ca = design_.cells[a];
    const Cell& cb = design_.cells[b];
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
                                         bool& has_best) const {
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
    xs.reserve(15);
    xs.push_back(snapped_pref);
    xs.push_back(clampInt64(snapUp(preferred, design_.die.x_min,
                                   design_.site_width),
                            start_min, start_max));
    xs.push_back(start_min);
    xs.push_back(start_max);
    for (int step = 1; step <= 5; ++step) {
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
      const double density_cost = density_.scoreCandidate(placed);
      Candidate cand{start_row, x, y,
                     alpha_ * disp_cost + (1.0 - alpha_) * density_cost,
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

    Candidate best;
    bool has_best = false;
    const std::vector<size_t> row_order = rowOrderForCell(cell);
    const size_t row_budget = (alpha_ < 0.20) ? 96 : ((alpha_ < 0.60) ? 64 : 32);
    size_t feasible_rows_seen = 0;
    for (size_t row_index : row_order) {
      if (evaluateCandidatesForRow(cell_index, row_index, best, has_best)) {
        ++feasible_rows_seen;
        if (has_best && feasible_rows_seen >= row_budget) break;
      }
    }

    if (!has_best) {
      error = "cannot place cell " + cell.name + " (" +
              std::to_string(rectWidth(cell.original)) + "x" +
              std::to_string(rectHeight(cell.original)) + " DBU)";
      return false;
    }

    commit(cell_index, best);
  }

  return true;
}

}  // namespace legalizer
