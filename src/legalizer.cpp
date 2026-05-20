#include "legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace legalizer {
namespace {

struct IntervalState {
  int row_index = 0;
  int interval_index = 0;
  RowInterval interval;
  std::vector<int> cells;
};

struct TrialPlacement {
  bool feasible = false;
  double score = 0.0;
  int state_index = -1;
  std::vector<std::pair<int, Rect>> placements;
};

Coord clampAligned(Coord value, Coord lo, Coord hi, const PlacementModel &model) {
  if (hi < lo) {
    return lo;
  }
  Coord aligned = nearestAligned(value, model.die.llx, model.site_width);
  if (aligned < lo) {
    return alignUp(lo, model.die.llx, model.site_width);
  }
  if (aligned > hi) {
    return alignDown(hi, model.die.llx, model.site_width);
  }
  return aligned;
}

bool orderBefore(const PlacementModel &model, int a, int b) {
  const Cell &ca = model.cells[static_cast<std::size_t>(a)];
  const Cell &cb = model.cells[static_cast<std::size_t>(b)];
  if (ca.original.llx != cb.original.llx) {
    return ca.original.llx < cb.original.llx;
  }
  if (ca.original.lly != cb.original.lly) {
    return ca.original.lly < cb.original.lly;
  }
  if (ca.name != cb.name) {
    return ca.name < cb.name;
  }
  return ca.input_index < cb.input_index;
}

std::vector<int> sortedSequenceWithCandidate(const PlacementModel &model,
                                             const IntervalState &state,
                                             int candidate_id) {
  std::vector<int> sequence = state.cells;
  sequence.push_back(candidate_id);
  std::sort(sequence.begin(), sequence.end(),
            [&](int a, int b) { return orderBefore(model, a, b); });
  return sequence;
}

bool solveIntervalSequence(const PlacementModel &model, const RowInterval &interval,
                           Coord row_y, const std::vector<int> &sequence,
                           std::vector<std::pair<int, Rect>> *placements) {
  const int n = static_cast<int>(sequence.size());
  if (n == 0) {
    return true;
  }

  std::vector<Coord> earliest(static_cast<std::size_t>(n));
  std::vector<Coord> latest(static_cast<std::size_t>(n));

  Coord cursor = interval.llx;
  for (int i = 0; i < n; ++i) {
    const Cell &cell = model.cells[static_cast<std::size_t>(sequence[i])];
    Coord lo = alignUp(cursor, model.die.llx, model.site_width);
    Coord hi = alignDown(interval.urx - width(cell.original), model.die.llx, model.site_width);
    if (lo > hi) {
      return false;
    }
    earliest[static_cast<std::size_t>(i)] = lo;
    cursor = lo + width(cell.original);
  }

  cursor = interval.urx;
  for (int i = n - 1; i >= 0; --i) {
    const Cell &cell = model.cells[static_cast<std::size_t>(sequence[i])];
    Coord hi = alignDown(cursor - width(cell.original), model.die.llx, model.site_width);
    if (hi < earliest[static_cast<std::size_t>(i)]) {
      return false;
    }
    latest[static_cast<std::size_t>(i)] = hi;
    cursor = hi;
  }

  std::vector<Coord> xs(static_cast<std::size_t>(n));
  Coord prev_end = interval.llx;
  for (int i = 0; i < n; ++i) {
    const Cell &cell = model.cells[static_cast<std::size_t>(sequence[i])];
    Coord lo = std::max(earliest[static_cast<std::size_t>(i)],
                        alignUp(prev_end, model.die.llx, model.site_width));
    Coord hi = latest[static_cast<std::size_t>(i)];
    if (lo > hi) {
      return false;
    }
    xs[static_cast<std::size_t>(i)] = clampAligned(cell.original.llx, lo, hi, model);
    prev_end = xs[static_cast<std::size_t>(i)] + width(cell.original);
  }

  placements->clear();
  placements->reserve(sequence.size());
  for (int i = 0; i < n; ++i) {
    int id = sequence[static_cast<std::size_t>(i)];
    const Cell &cell = model.cells[static_cast<std::size_t>(id)];
    placements->push_back(std::make_pair(id, movedRect(cell.original, xs[static_cast<std::size_t>(i)], row_y)));
  }
  return true;
}

TrialPlacement makeTrial(const PlacementModel &model, const std::vector<SiteRow> &rows,
                         const std::vector<IntervalState> &states, const DensityGrid &density,
                         const LegalizeOptions &options, int cell_id, int state_index) {
  TrialPlacement trial;
  trial.state_index = state_index;
  const Cell &cell = model.cells[static_cast<std::size_t>(cell_id)];
  const IntervalState &state = states[static_cast<std::size_t>(state_index)];
  if (width(cell.original) > width(Rect{state.interval.llx, 0, state.interval.urx, 1})) {
    return trial;
  }

  std::vector<int> sequence = sortedSequenceWithCandidate(model, state, cell_id);
  if (!solveIntervalSequence(model, state.interval,
                             rows[static_cast<std::size_t>(state.row_index)].y,
                             sequence, &trial.placements)) {
    return trial;
  }

  double movement_delta = 0.0;
  double disturbance = 0.0;
  Rect candidate_rect;
  for (const auto &entry : trial.placements) {
    const Cell &placed_cell = model.cells[static_cast<std::size_t>(entry.first)];
    double before = placed_cell.has_placement
                        ? manhattanMicron(model, placed_cell.original, placed_cell.placed)
                        : 0.0;
    double after = manhattanMicron(model, placed_cell.original, entry.second);
    movement_delta += after - before;
    if (placed_cell.has_placement) {
      disturbance += manhattanMicron(model, placed_cell.placed, entry.second);
    }
    if (entry.first == cell_id) {
      candidate_rect = entry.second;
    }
  }

  double density_penalty = density.trialPenalty(candidate_rect, options.threshold);
  double row_distance = std::abs(candidate_rect.lly - cell.original.lly) /
                        static_cast<double>(model.dbu_per_micron);
  double movement_cost = options.norm_factor * (movement_delta + 0.01 * disturbance +
                                                0.05 * row_distance);
  trial.score = options.alpha * movement_cost +
                (1.0 - options.alpha) * density_penalty;
  trial.feasible = true;
  return trial;
}

void commitTrial(PlacementModel *model, std::vector<IntervalState> *states, DensityGrid *density,
                 const TrialPlacement &trial) {
  IntervalState &state = (*states)[static_cast<std::size_t>(trial.state_index)];
  state.cells.clear();
  for (const auto &entry : trial.placements) {
    Cell &cell = model->cells[static_cast<std::size_t>(entry.first)];
    if (cell.has_placement) {
      density->addRect(cell.placed, -1.0);
    }
    cell.placed = entry.second;
    cell.has_placement = true;
    density->addRect(cell.placed, 1.0);
    state.cells.push_back(entry.first);
  }
}

std::vector<IntervalState> makeStates(const std::vector<SiteRow> &rows) {
  std::vector<IntervalState> states;
  for (std::size_t r = 0; r < rows.size(); ++r) {
    for (std::size_t i = 0; i < rows[r].intervals.size(); ++i) {
      IntervalState state;
      state.row_index = static_cast<int>(r);
      state.interval_index = static_cast<int>(i);
      state.interval = rows[r].intervals[i];
      states.push_back(state);
    }
  }
  return states;
}

std::vector<std::vector<int>> statesByRow(const std::vector<IntervalState> &states,
                                          std::size_t row_count) {
  std::vector<std::vector<int>> by_row(row_count);
  for (std::size_t i = 0; i < states.size(); ++i) {
    by_row[static_cast<std::size_t>(states[i].row_index)].push_back(static_cast<int>(i));
  }
  return by_row;
}

std::vector<int> nearbyCandidateStates(const PlacementModel &model, const std::vector<SiteRow> &rows,
                                       const std::vector<std::vector<int>> &by_row,
                                       const Cell &cell) {
  std::vector<int> candidates;
  if (rows.empty()) {
    return candidates;
  }
  Coord raw_row = (cell.original.lly - model.die.lly) / model.site_height;
  int origin_row = static_cast<int>(std::max<Coord>(0, std::min<Coord>(
      static_cast<Coord>(rows.size() - 1), raw_row)));
  const int max_radius = std::min<int>(static_cast<int>(rows.size()) - 1, 25);
  for (int radius = 0; radius <= max_radius; ++radius) {
    int lo = origin_row - radius;
    int hi = origin_row + radius;
    if (lo >= 0) {
      candidates.insert(candidates.end(), by_row[static_cast<std::size_t>(lo)].begin(),
                        by_row[static_cast<std::size_t>(lo)].end());
    }
    if (radius != 0 && hi < static_cast<int>(rows.size())) {
      candidates.insert(candidates.end(), by_row[static_cast<std::size_t>(hi)].begin(),
                        by_row[static_cast<std::size_t>(hi)].end());
    }
  }
  return candidates;
}

LegalizeResult legalizeOnePass(const PlacementModel &input, const std::vector<SiteRow> &rows,
                               const LegalizeOptions &options, bool reverse) {
  LegalizeResult result;
  result.model = input;
  for (Cell &cell : result.model.cells) {
    cell.has_placement = false;
    cell.placed = cell.original;
  }

  for (const Cell &cell : result.model.cells) {
    if (!isSingleRowCell(result.model, cell)) {
      result.error = "unsupported multi-row movable cell '" + cell.name + "' height " +
                     std::to_string(height(cell.original)) + " site height " +
                     std::to_string(result.model.site_height);
      return result;
    }
  }

  std::vector<IntervalState> states = makeStates(rows);
  if (states.empty()) {
    result.error = "no legal row intervals are available";
    return result;
  }
  std::vector<std::vector<int>> by_row = statesByRow(states, rows.size());

  DensityGrid density(result.model);
  for (int cell_id : cellOrder(result.model, reverse)) {
    TrialPlacement best;
    const Cell &cell = result.model.cells[static_cast<std::size_t>(cell_id)];
    std::vector<int> candidates = nearbyCandidateStates(result.model, rows, by_row, cell);
    for (int state_index : candidates) {
      TrialPlacement trial =
          makeTrial(result.model, rows, states, density, options, cell_id,
                    state_index);
      if (!trial.feasible) {
        continue;
      }
      if (!best.feasible || trial.score < best.score ||
          (trial.score == best.score && trial.state_index < best.state_index)) {
        best = std::move(trial);
      }
    }
    if (!best.feasible) {
      for (std::size_t state_index = 0; state_index < states.size(); ++state_index) {
        TrialPlacement trial =
            makeTrial(result.model, rows, states, density, options, cell_id,
                      static_cast<int>(state_index));
        if (!trial.feasible) {
          continue;
        }
        if (!best.feasible || trial.score < best.score ||
            (trial.score == best.score && trial.state_index < best.state_index)) {
          best = std::move(trial);
        }
      }
    }
    if (!best.feasible) {
      result.error = "no legal interval can fit cell '" + cell.name + "' width " +
                     std::to_string(width(cell.original));
      return result;
    }
    commitTrial(&result.model, &states, &density, best);
  }

  ValidationResult validation = validatePlacement(result.model, rows, options);
  if (!validation.ok) {
    result.error = validation.error;
    return result;
  }
  result.ok = true;
  result.average_displacement = validation.average_displacement;
  result.dor = validation.dor;
  result.quality = validation.quality;
  return result;
}

bool rectInsideInterval(const Rect &rect, const SiteRow &row) {
  if (rect.lly != row.y) {
    return false;
  }
  for (const RowInterval &interval : row.intervals) {
    if (interval.llx <= rect.llx && rect.urx <= interval.urx) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::vector<int> cellOrder(const PlacementModel &model, bool reverse) {
  std::vector<int> order;
  order.reserve(model.cells.size());
  for (std::size_t i = 0; i < model.cells.size(); ++i) {
    order.push_back(static_cast<int>(i));
  }
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    const Cell &ca = model.cells[static_cast<std::size_t>(a)];
    const Cell &cb = model.cells[static_cast<std::size_t>(b)];
    if (ca.original.llx != cb.original.llx) {
      return reverse ? ca.original.llx > cb.original.llx : ca.original.llx < cb.original.llx;
    }
    if (ca.original.lly != cb.original.lly) {
      return ca.original.lly < cb.original.lly;
    }
    if (ca.name != cb.name) {
      return ca.name < cb.name;
    }
    return ca.input_index < cb.input_index;
  });
  return order;
}

double averageDisplacementMicron(const PlacementModel &model) {
  if (model.cells.empty()) {
    return 0.0;
  }
  double total = 0.0;
  for (const Cell &cell : model.cells) {
    total += manhattanMicron(model, cell.original, cell.placed);
  }
  return total / static_cast<double>(model.cells.size());
}

ValidationResult validatePlacement(const PlacementModel &model, const std::vector<SiteRow> &rows,
                                   const LegalizeOptions &options) {
  ValidationResult result;
  std::set<std::pair<Coord, Coord>> seen;
  for (std::size_t i = 0; i < model.cells.size(); ++i) {
    const Cell &cell = model.cells[i];
    if (!cell.has_placement) {
      result.error = "cell '" + cell.name + "' has no placement";
      return result;
    }
    if (!contains(model.die, cell.placed)) {
      result.error = "cell '" + cell.name + "' is outside the die";
      return result;
    }
    if (!isSiteAlignedX(model, cell.placed.llx)) {
      result.error = "cell '" + cell.name + "' is not site-column aligned";
      return result;
    }
    if (!isRowAlignedY(model, cell.placed.lly)) {
      result.error = "cell '" + cell.name + "' is not row aligned";
      return result;
    }
    bool in_row_interval = false;
    for (const SiteRow &row : rows) {
      if (rectInsideInterval(cell.placed, row)) {
        in_row_interval = true;
        break;
      }
    }
    if (!in_row_interval) {
      result.error = "cell '" + cell.name + "' is not inside a legal row interval";
      return result;
    }
    for (const Obstacle &obstacle : model.obstacles) {
      if (overlaps(cell.placed, obstacle.rect)) {
        result.error = "cell '" + cell.name + "' overlaps obstacle '" + obstacle.name + "'";
        return result;
      }
    }
    for (std::size_t j = 0; j < i; ++j) {
      if (overlaps(cell.placed, model.cells[j].placed)) {
        result.error = "cell '" + cell.name + "' overlaps cell '" + model.cells[j].name + "'";
        return result;
      }
    }
  }

  DensityResult density = computeFinalDensity(model, options.threshold);
  result.average_displacement = averageDisplacementMicron(model);
  result.dor = density.dor;
  result.quality = options.alpha * (result.average_displacement * options.norm_factor) +
                   (1.0 - options.alpha) * result.dor;
  result.ok = true;
  return result;
}

LegalizeResult legalizePlacement(const PlacementModel &model, const std::vector<SiteRow> &rows,
                                 const LegalizeOptions &options) {
  LegalizeResult forward = legalizeOnePass(model, rows, options, false);
  LegalizeResult reverse = legalizeOnePass(model, rows, options, true);
  if (forward.ok && reverse.ok) {
    return reverse.quality < forward.quality ? reverse : forward;
  }
  if (forward.ok) {
    return forward;
  }
  if (reverse.ok) {
    return reverse;
  }
  LegalizeResult failed;
  failed.error = !forward.error.empty() ? forward.error : reverse.error;
  return failed;
}

}  // namespace legalizer
