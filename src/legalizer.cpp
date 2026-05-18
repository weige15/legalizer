#include "legalizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace legalizer {
namespace {

constexpr double kNormFactor = 18.2;
constexpr int64_t kInitialRadiusSites = 20;
constexpr size_t kMaxRefinementRounds = 2;
constexpr size_t kMaxRefinementOutliers = 96;
constexpr size_t kRefinementRowBudget = 96;
constexpr size_t kPartnerRowBudget = 48;
constexpr double kDensityEps = 1e-6;

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

std::vector<int64_t> Legalizer::xCandidatesForInterval(
    const Cell& cell, const RowInterval& interval, int64_t width,
    int64_t radius_sites) const {
  const int64_t start_min =
      snapUp(interval.x_min, design_.die.x_min, design_.site_width);
  const int64_t start_max =
      snapDown(interval.x_max - width, design_.die.x_min, design_.site_width);
  if (start_min > start_max) return {};

  const int64_t preferred = clampInt64(cell.original.x_min, start_min, start_max);
  const int64_t snapped_down =
      clampInt64(snapDown(preferred, design_.die.x_min, design_.site_width),
                 start_min, start_max);
  const int64_t snapped_up =
      clampInt64(snapUp(preferred, design_.die.x_min, design_.site_width),
                 start_min, start_max);

  std::vector<int64_t> xs;
  xs.reserve(static_cast<size_t>(std::min<int64_t>(radius_sites * 2 + 6, 512)));
  xs.push_back(snapped_down);
  xs.push_back(snapped_up);
  xs.push_back(start_min);
  xs.push_back(start_max);

  const int64_t dense_radius = std::min<int64_t>(radius_sites, 64);
  for (int64_t step = 1; step <= dense_radius; ++step) {
    const int64_t delta = step * design_.site_width;
    if (snapped_down - delta >= start_min) xs.push_back(snapped_down - delta);
    if (snapped_up + delta <= start_max) xs.push_back(snapped_up + delta);
  }
  for (int64_t step = dense_radius + 16; step <= radius_sites; step += 16) {
    const int64_t delta = step * design_.site_width;
    if (snapped_down - delta >= start_min) xs.push_back(snapped_down - delta);
    if (snapped_up + delta <= start_max) xs.push_back(snapped_up + delta);
  }

  std::sort(xs.begin(), xs.end());
  xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
  return xs;
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

bool Legalizer::isBetterRefinement(const RefinementMove& cand,
                                   const RefinementMove& best) const {
  constexpr double eps = 1e-9;
  if (cand.max_displacement_cost + eps < best.max_displacement_cost) return true;
  if (std::abs(cand.max_displacement_cost - best.max_displacement_cost) > eps) {
    return false;
  }
  if (cand.total_displacement_cost + eps < best.total_displacement_cost) {
    return true;
  }
  if (std::abs(cand.total_displacement_cost - best.total_displacement_cost) >
      eps) {
    return false;
  }
  if (cand.density_cost + eps < best.density_cost) return true;
  if (std::abs(cand.density_cost - best.density_cost) > eps) return false;
  if (cand.cell_rect.y_min != best.cell_rect.y_min) {
    return cand.cell_rect.y_min < best.cell_rect.y_min;
  }
  return cand.cell_rect.x_min < best.cell_rect.x_min;
}

bool Legalizer::evaluateCandidatesForRow(size_t cell_index, size_t start_row,
                                         int64_t radius_sites, Candidate& best,
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
    const std::vector<int64_t> xs =
        xCandidatesForInterval(cell, interval, width, radius_sites);
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

bool Legalizer::findBestPlacement(size_t cell_index, size_t row_budget,
                                  int64_t radius_sites, Candidate& best,
                                  bool& has_best) const {
  const Cell& cell = design_.cells[cell_index];
  bool found_any = false;
  size_t feasible_rows_seen = 0;
  for (size_t row_index : rowOrderForCell(cell)) {
    if (evaluateCandidatesForRow(cell_index, row_index, radius_sites, best,
                                 has_best)) {
      found_any = true;
      ++feasible_rows_seen;
      if (has_best && feasible_rows_seen >= row_budget) break;
    }
  }
  return found_any;
}

void Legalizer::addOccupancy(size_t cell_index, const Rect& rect,
                             size_t start_row) {
  const int64_t row_span = rectHeight(rect) / design_.site_height;
  for (int64_t offset = 0; offset < row_span; ++offset) {
    auto& segments = occupancy_[start_row + static_cast<size_t>(offset)];
    segments.push_back(Segment{rect.x_min, rect.x_max, cell_index});
    std::sort(segments.begin(), segments.end(), [](const Segment& a,
                                                   const Segment& b) {
      if (a.x_min != b.x_min) return a.x_min < b.x_min;
      if (a.x_max != b.x_max) return a.x_max < b.x_max;
      return a.cell_index < b.cell_index;
    });
  }
}

void Legalizer::removeOccupancy(const Rect& rect, size_t start_row) {
  const int64_t row_span = rectHeight(rect) / design_.site_height;
  for (int64_t offset = 0; offset < row_span; ++offset) {
    auto& segments = occupancy_[start_row + static_cast<size_t>(offset)];
    const auto it = std::find_if(
        segments.begin(), segments.end(), [&rect](const Segment& segment) {
          return segment.x_min == rect.x_min && segment.x_max == rect.x_max;
        });
    if (it != segments.end()) segments.erase(it);
  }
}

bool Legalizer::rowIndexForY(int64_t y, size_t& row_index) const {
  if (y < design_.die.y_min || y >= design_.die.y_max) return false;
  const int64_t offset = y - design_.die.y_min;
  if (offset % design_.site_height != 0) return false;
  const int64_t index = offset / design_.site_height;
  if (index < 0 || static_cast<size_t>(index) >= rows_.size()) return false;
  if (rows_[static_cast<size_t>(index)].y != y) return false;
  row_index = static_cast<size_t>(index);
  return true;
}

bool Legalizer::placementIsLegalInCurrentOccupancy(const Cell& cell,
                                                   const Rect& rect) const {
  if (!contains(design_.die, rect)) return false;
  if (rectWidth(rect) != rectWidth(cell.original) ||
      rectHeight(rect) != rectHeight(cell.original)) {
    return false;
  }

  size_t start_row = 0;
  if (!rowIndexForY(rect.y_min, start_row)) return false;
  const int64_t row_span = rectHeight(rect) / design_.site_height;
  if (row_span <= 0 || start_row + static_cast<size_t>(row_span) > rows_.size()) {
    return false;
  }

  bool fixed_free = false;
  for (const RowInterval& interval : commonFreeIntervals(start_row, row_span)) {
    if (intervalContains(interval, rect.x_min, rectWidth(rect))) {
      fixed_free = true;
      break;
    }
  }
  if (!fixed_free) return false;

  for (int64_t offset = 0; offset < row_span; ++offset) {
    if (segmentOverlaps(occupancy_[start_row + static_cast<size_t>(offset)],
                        rect.x_min, rect.x_max)) {
      return false;
    }
  }
  return true;
}

std::vector<size_t> Legalizer::overlappingOccupancyOwners(
    const Rect& rect, size_t ignore_index, size_t limit) const {
  std::vector<size_t> out;
  size_t start_row = 0;
  if (!rowIndexForY(rect.y_min, start_row)) return out;
  const int64_t row_span = rectHeight(rect) / design_.site_height;
  if (row_span <= 0 || start_row + static_cast<size_t>(row_span) > rows_.size()) {
    return out;
  }

  for (int64_t offset = 0; offset < row_span; ++offset) {
    const auto& segments = occupancy_[start_row + static_cast<size_t>(offset)];
    const auto begin = std::lower_bound(
        segments.begin(), segments.end(), rect.x_min,
        [](const Segment& segment, int64_t value) {
          return segment.x_max <= value;
        });
    for (auto it = begin; it != segments.end() && it->x_min < rect.x_max; ++it) {
      if (it->cell_index == ignore_index) continue;
      if (std::find(out.begin(), out.end(), it->cell_index) == out.end()) {
        out.push_back(it->cell_index);
        if (out.size() > limit) return out;
      }
    }
  }
  return out;
}

double Legalizer::displacementCost(const Cell& cell, const Rect& rect) const {
  const double disp_um =
      static_cast<double>(manhattanDisplacement(cell.original, rect)) /
      static_cast<double>(design_.dbu_per_micron);
  return disp_um * kNormFactor;
}

void Legalizer::commit(size_t cell_index, const Candidate& candidate) {
  Cell& cell = design_.cells[cell_index];
  cell.placed = makeRect(candidate.x, candidate.y, rectWidth(cell.original),
                         rectHeight(cell.original));
  cell.has_placement = true;

  addOccupancy(cell_index, cell.placed, candidate.row_index);
  density_.commit(cell.placed);
}

bool Legalizer::evaluateEmptyRefinement(size_t cell_index,
                                        const Rect& old_rect,
                                        const Rect& new_rect,
                                        RefinementMove& best,
                                        bool& has_best) const {
  const Cell& cell = design_.cells[cell_index];
  const double old_disp = displacementCost(cell, old_rect);
  const double new_disp = displacementCost(cell, new_rect);
  if (new_disp + 1e-9 >= old_disp) return false;

  const double old_density = density_.scoreCandidate(old_rect);
  const double new_density = density_.scoreCandidate(new_rect);
  if (new_density > old_density + kDensityEps) return false;

  const double old_total = alpha_ * old_disp + (1.0 - alpha_) * old_density;
  const double new_total = alpha_ * new_disp + (1.0 - alpha_) * new_density;
  if (new_total + 1e-9 >= old_total) return false;

  RefinementMove move;
  move.cell_rect = new_rect;
  move.max_displacement_cost = new_disp;
  move.total_displacement_cost = new_disp;
  move.density_cost = new_density;
  move.total_cost = new_total;
  if (!has_best || isBetterRefinement(move, best)) {
    best = move;
    has_best = true;
  }
  return true;
}

bool Legalizer::evaluateSwapRefinement(size_t cell_index, size_t partner_index,
                                       const Rect& old_rect,
                                       const Rect& new_rect,
                                       double outlier_threshold_um,
                                       RefinementMove& best,
                                       bool& has_best) {
  const Cell& cell = design_.cells[cell_index];
  const Cell& partner = design_.cells[partner_index];
  const Rect partner_old = partner.placed;
  size_t partner_row = 0;
  if (!rowIndexForY(partner_old.y_min, partner_row)) return false;

  removeOccupancy(partner_old, partner_row);
  density_.uncommit(partner_old);

  bool accepted = false;
  if (placementIsLegalInCurrentOccupancy(cell, new_rect)) {
    size_t new_row = 0;
    if (rowIndexForY(new_rect.y_min, new_row)) {
      const double new_cell_density = density_.scoreCandidate(new_rect);
      addOccupancy(cell_index, new_rect, new_row);
      density_.commit(new_rect);

      Candidate partner_best;
      bool has_partner = false;
      findBestPlacement(partner_index, kPartnerRowBudget, 96, partner_best,
                        has_partner);

      density_.uncommit(new_rect);
      removeOccupancy(new_rect, new_row);

      if (has_partner) {
        const Rect partner_new =
            makeRect(partner_best.x, partner_best.y, rectWidth(partner.original),
                     rectHeight(partner.original));
        if (!intersects(new_rect, partner_new)) {
          const double old_cell_disp = displacementCost(cell, old_rect);
          const double old_partner_disp = displacementCost(partner, partner_old);
          const double new_cell_disp = displacementCost(cell, new_rect);
          const double new_partner_disp = displacementCost(partner, partner_new);
          const double severe_limit = outlier_threshold_um * kNormFactor;
          if (new_partner_disp <= severe_limit + 1e-9 &&
              std::max(new_cell_disp, new_partner_disp) + 1e-9 <
                  std::max(old_cell_disp, old_partner_disp) &&
              new_cell_disp + new_partner_disp + 1e-9 <
                  old_cell_disp + old_partner_disp) {
            const double old_cell_density = density_.scoreCandidate(old_rect);
            density_.commit(old_rect);
            const double old_partner_density = density_.scoreCandidate(partner_old);
            density_.uncommit(old_rect);

            const double old_density = old_cell_density + old_partner_density;
            const double new_density = new_cell_density + partner_best.density_cost;
            if (new_density <= old_density + kDensityEps) {
              const double old_total =
                  alpha_ * (old_cell_disp + old_partner_disp) +
                  (1.0 - alpha_) * old_density;
              const double new_total =
                  alpha_ * (new_cell_disp + new_partner_disp) +
                  (1.0 - alpha_) * new_density;
              if (new_total + 1e-9 < old_total) {
                RefinementMove move;
                move.swaps_partner = true;
                move.partner_index = partner_index;
                move.cell_rect = new_rect;
                move.partner_rect = partner_new;
                move.max_displacement_cost =
                    std::max(new_cell_disp, new_partner_disp);
                move.total_displacement_cost = new_cell_disp + new_partner_disp;
                move.density_cost = new_density;
                move.total_cost = new_total;
                if (!has_best || isBetterRefinement(move, best)) {
                  best = move;
                  has_best = true;
                }
                accepted = true;
              }
            }
          }
        }
      }
    }
  }

  density_.commit(partner_old);
  addOccupancy(partner_index, partner_old, partner_row);
  return accepted;
}

bool Legalizer::tryRefineOutlier(size_t cell_index,
                                 double outlier_threshold_um) {
  Cell& cell = design_.cells[cell_index];
  const Rect old_rect = cell.placed;
  size_t old_row = 0;
  if (!rowIndexForY(old_rect.y_min, old_row)) return false;

  removeOccupancy(old_rect, old_row);
  density_.uncommit(old_rect);

  RefinementMove best;
  bool has_best = false;
  const int64_t old_disp_dbu = manhattanDisplacement(cell.original, old_rect);
  const int64_t dynamic_radius =
      std::min<int64_t>(4096, std::max<int64_t>(
                                  256, ceilDiv(old_disp_dbu, design_.site_width) +
                                           64));

  size_t rows_seen = 0;
  const int64_t row_span = rectHeight(cell.original) / design_.site_height;
  for (size_t row_index : rowOrderForCell(cell)) {
    if (++rows_seen > kRefinementRowBudget) break;
    const std::vector<RowInterval> fixed_intervals =
        commonFreeIntervals(row_index, row_span);
    for (const RowInterval& interval : fixed_intervals) {
      const std::vector<int64_t> xs = xCandidatesForInterval(
          cell, interval, rectWidth(cell.original), dynamic_radius);
      for (int64_t x : xs) {
        const Rect new_rect =
            makeRect(x, rows_[row_index].y, rectWidth(cell.original),
                     rectHeight(cell.original));
        if (new_rect.x_min == old_rect.x_min &&
            new_rect.y_min == old_rect.y_min) {
          continue;
        }
        if (!contains(design_.die, new_rect)) continue;

        if (placementIsLegalInCurrentOccupancy(cell, new_rect)) {
          evaluateEmptyRefinement(cell_index, old_rect, new_rect, best, has_best);
        } else {
          const std::vector<size_t> blockers =
              overlappingOccupancyOwners(new_rect, cell_index, 1);
          if (blockers.size() == 1) {
            evaluateSwapRefinement(cell_index, blockers[0], old_rect, new_rect,
                                   outlier_threshold_um, best, has_best);
          }
        }
      }
    }
  }

  if (!has_best) {
    density_.commit(old_rect);
    addOccupancy(cell_index, old_rect, old_row);
    return false;
  }

  if (best.swaps_partner) {
    Cell& partner = design_.cells[best.partner_index];
    const Rect partner_old = partner.placed;
    size_t partner_old_row = 0;
    rowIndexForY(partner_old.y_min, partner_old_row);
    removeOccupancy(partner_old, partner_old_row);
    density_.uncommit(partner_old);

    cell.placed = best.cell_rect;
    size_t new_row = 0;
    rowIndexForY(best.cell_rect.y_min, new_row);
    density_.commit(best.cell_rect);
    addOccupancy(cell_index, best.cell_rect, new_row);

    partner.placed = best.partner_rect;
    size_t partner_new_row = 0;
    rowIndexForY(best.partner_rect.y_min, partner_new_row);
    density_.commit(best.partner_rect);
    addOccupancy(best.partner_index, best.partner_rect, partner_new_row);
  } else {
    cell.placed = best.cell_rect;
    size_t new_row = 0;
    rowIndexForY(best.cell_rect.y_min, new_row);
    density_.commit(best.cell_rect);
    addOccupancy(cell_index, best.cell_rect, new_row);
  }

  return true;
}

bool Legalizer::refineOutliers() {
  bool changed_any = false;
  for (size_t round = 0; round < kMaxRefinementRounds; ++round) {
    double total_um = 0.0;
    std::vector<std::pair<double, size_t>> outliers;
    outliers.reserve(design_.cells.size());
    for (size_t i = 0; i < design_.cells.size(); ++i) {
      const Cell& cell = design_.cells[i];
      const double disp_um =
          static_cast<double>(manhattanDisplacement(cell.original, cell.placed)) /
          static_cast<double>(design_.dbu_per_micron);
      total_um += disp_um;
      outliers.push_back({disp_um, i});
    }

    const double avg_um =
        design_.cells.empty() ? 0.0 : total_um / design_.cells.size();
    const double threshold_um = std::max(45.0, 3.0 * avg_um);
    outliers.erase(std::remove_if(outliers.begin(), outliers.end(),
                                  [threshold_um](const auto& item) {
                                    return item.first <= threshold_um;
                                  }),
                   outliers.end());
    if (outliers.empty()) break;

    std::stable_sort(outliers.begin(), outliers.end(),
                     [this](const auto& a, const auto& b) {
                       if (a.first != b.first) return a.first > b.first;
                       return design_.cells[a.second].input_index <
                              design_.cells[b.second].input_index;
                     });
    if (outliers.size() > kMaxRefinementOutliers) {
      outliers.resize(kMaxRefinementOutliers);
    }

    bool round_changed = false;
    for (const auto& item : outliers) {
      const size_t cell_index = item.second;
      const Cell& cell = design_.cells[cell_index];
      const double current_um =
          static_cast<double>(manhattanDisplacement(cell.original, cell.placed)) /
          static_cast<double>(design_.dbu_per_micron);
      if (current_um <= threshold_um) continue;
      if (tryRefineOutlier(cell_index, threshold_um)) {
        round_changed = true;
        changed_any = true;
      }
    }
    if (!round_changed) break;
  }
  return changed_any;
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
    const size_t row_budget = (alpha_ < 0.20) ? 96 : ((alpha_ < 0.60) ? 64 : 32);
    findBestPlacement(cell_index, row_budget, kInitialRadiusSites, best,
                      has_best);

    if (!has_best) {
      error = "cannot place cell " + cell.name + " (" +
              std::to_string(rectWidth(cell.original)) + "x" +
              std::to_string(rectHeight(cell.original)) + " DBU)";
      return false;
    }

    commit(cell_index, best);
  }

  refineOutliers();
  return true;
}

}  // namespace legalizer
