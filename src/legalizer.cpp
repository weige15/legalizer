#include "legalizer.h"

#include "density_estimator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>

namespace legalizer {

namespace {

constexpr int kInitialRowWindow = 8;
constexpr int kMaxCandidateTrials = 80;
constexpr int kRepairCells = 80;
constexpr int kRepairPasses = 2;
constexpr double kEdgePenaltyWeight = 0.0001;

struct Cluster {
    std::vector<size_t> ids;
    int64_t width = 0;
    int64_t weight = 0;
    long double q = 0.0;
    int64_t x = 0;
};

struct PassState {
    std::vector<Point> placements;
    std::vector<bool> placed;
    std::vector<std::vector<size_t>> assigned;
    std::vector<int> cell_interval;
    std::vector<int64_t> used_width;
};

Point originalPoint(const PlacementModel& model, size_t id) {
    const Rect& r = model.instances[id].original;
    return Point{r.x0, r.y0};
}

int64_t rounded(long double v) {
    return static_cast<int64_t>(std::llround(v));
}

void collapseCluster(const PlacementModel& model, const RowInterval& interval, Cluster* cluster) {
    const int64_t max_x = snapDownToGrid(interval.x1 - cluster->width, model.die.x0, model.site_width);
    int64_t x = rounded(cluster->q / static_cast<long double>(cluster->weight));
    x = snapDownToGrid(x + model.site_width / 2, model.die.x0, model.site_width);
    x = std::max(interval.x0, std::min(max_x, x));
    cluster->x = x;
}

Cluster mergeClusters(const RowInterval& interval, const PlacementModel& model, const Cluster& a,
                      const Cluster& b) {
    (void)model;
    Cluster merged;
    merged.ids = a.ids;
    merged.ids.insert(merged.ids.end(), b.ids.begin(), b.ids.end());
    merged.width = a.width + b.width;
    merged.weight = a.weight + b.weight;
    merged.q = a.q + b.q - static_cast<long double>(a.width) * b.weight;
    collapseCluster(model, interval, &merged);
    return merged;
}

std::vector<int> candidateIntervals(const PlacementModel& model,
                                    const std::vector<RowInterval>& intervals, size_t cell_id,
                                    int64_t cell_width, const PassState& state,
                                    bool full_fallback) {
    std::vector<int> ids;
    int target_row = model.rowIndexForY(model.instances[cell_id].original.y0);
    if (target_row < 0) {
        const int64_t rel = model.instances[cell_id].original.y0 - model.die.y0;
        target_row = static_cast<int>((rel + model.site_height / 2) / model.site_height);
        target_row = std::max(0, std::min(model.rowCount() - 1, target_row));
    }
    for (size_t i = 0; i < intervals.size(); ++i) {
        const auto& interval = intervals[i];
        if (interval.x1 - interval.x0 < cell_width) {
            continue;
        }
        if (state.used_width[i] + cell_width > interval.x1 - interval.x0) {
            continue;
        }
        if (!full_fallback && target_row >= 0 &&
            std::abs(interval.row_index - target_row) > kInitialRowWindow) {
            continue;
        }
        ids.push_back(static_cast<int>(i));
    }
    std::sort(ids.begin(), ids.end(), [&](int a, int b) {
        const auto& ia = intervals[a];
        const auto& ib = intervals[b];
        const int da = target_row >= 0 ? std::abs(ia.row_index - target_row) : 0;
        const int db = target_row >= 0 ? std::abs(ib.row_index - target_row) : 0;
        if (da != db) return da < db;
        const int64_t cx = model.instances[cell_id].original.x0;
        const int64_t ca = std::max<int64_t>(0, std::max(ia.x0 - cx, cx - ia.x1));
        const int64_t cb = std::max<int64_t>(0, std::max(ib.x0 - cx, cx - ib.x1));
        if (ca != cb) return ca < cb;
        return a < b;
    });
    return ids;
}

double rowLoadPenalty(const RowInterval& interval, int64_t used_after) {
    const double width = static_cast<double>(interval.x1 - interval.x0);
    if (width <= 0.0) {
        return 1e9;
    }
    const double load = static_cast<double>(used_after) / width;
    return load * load * 100.0;
}

void applySolvedRow(const PlacementModel& model, int interval_id,
                    const std::vector<RowInterval>& intervals, const std::vector<Point>& solved,
                    PassState* state) {
    const auto& ids = state->assigned[interval_id];
    for (size_t id : ids) {
        state->placements[id] = solved[id];
        state->placed[id] = true;
        state->cell_interval[id] = interval_id;
    }
    (void)model;
    (void)intervals;
}

bool insertCell(const PlacementModel& model, const std::vector<RowInterval>& intervals, size_t cell_id,
                double alpha, double threshold, PassState* state, std::string* error) {
    const int64_t cell_width = model.width(model.instances[cell_id]);
    std::vector<int> cands = candidateIntervals(model, intervals, cell_id, cell_width, *state, false);
    if (cands.empty()) {
        cands = candidateIntervals(model, intervals, cell_id, cell_width, *state, true);
    }
    double best_score = std::numeric_limits<double>::infinity();
    int best_interval = -1;
    std::vector<Point> best_solved;
    int trials = 0;

    for (int interval_id : cands) {
        if (trials++ >= kMaxCandidateTrials && best_interval >= 0) {
            break;
        }
        std::vector<size_t> trial_cells = state->assigned[interval_id];
        trial_cells.push_back(cell_id);
        std::vector<Point> solved = solveRowInterval(model, intervals[interval_id], trial_cells);

        double delta_dbu = 0.0;
        for (size_t id : trial_cells) {
            const Point orig = originalPoint(model, id);
            const double new_d = static_cast<double>(manhattan(orig, solved[id]));
            const double old_d = state->placed[id] ? static_cast<double>(manhattan(orig, state->placements[id])) : 0.0;
            delta_dbu += new_d - old_d;
        }
        const double norm_delta =
            (delta_dbu / static_cast<double>(model.dbu_per_micron)) * 18.2 /
            static_cast<double>(std::max<size_t>(1, model.cell_ids.size()));
        const double density = estimateLocalDensityPenalty(model, state->placements, state->placed,
                                                           cell_id, solved[cell_id], threshold);
        const double load = rowLoadPenalty(intervals[interval_id], state->used_width[interval_id] + cell_width);
        const double edge =
            (solved[cell_id].x == intervals[interval_id].x0 ||
             solved[cell_id].x + cell_width == intervals[interval_id].x1)
                ? kEdgePenaltyWeight
                : 0.0;
        const double score = alpha * norm_delta + (1.0 - alpha) * (0.45 * density + 0.55 * load) +
                             edge + static_cast<double>(interval_id) * 1e-12;
        if (score < best_score) {
            best_score = score;
            best_interval = interval_id;
            best_solved = std::move(solved);
        }
    }

    if (best_interval < 0) {
        *error = "no feasible legal interval for " + model.instances[cell_id].name;
        return false;
    }
    state->assigned[best_interval].push_back(cell_id);
    state->used_width[best_interval] += cell_width;
    applySolvedRow(model, best_interval, intervals, best_solved, state);
    return true;
}

std::vector<size_t> makeOrder(const PlacementModel& model, int variant, double alpha,
                              double threshold) {
    (void)threshold;
    std::vector<size_t> order = model.cell_ids;
    if (variant == 0) {
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            const auto& ia = model.instances[a];
            const auto& ib = model.instances[b];
            if (ia.original.x0 != ib.original.x0) return ia.original.x0 < ib.original.x0;
            if (ia.original.y0 != ib.original.y0) return ia.original.y0 < ib.original.y0;
            return ia.input_order < ib.input_order;
        });
    } else if (variant == 1) {
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            const auto& ia = model.instances[a];
            const auto& ib = model.instances[b];
            if (ia.original.x0 != ib.original.x0) return ia.original.x0 > ib.original.x0;
            if (ia.original.y0 != ib.original.y0) return ia.original.y0 < ib.original.y0;
            return ia.input_order < ib.input_order;
        });
    } else if (variant == 2 && alpha < 0.85) {
        const int64_t grid = 10 * model.dbu_per_micron;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            const auto& ia = model.instances[a];
            const auto& ib = model.instances[b];
            const int64_t ba = ((ia.original.x0 - model.die.x0) / grid) +
                               131 * ((ia.original.y0 - model.die.y0) / grid);
            const int64_t bb = ((ib.original.x0 - model.die.x0) / grid) +
                               131 * ((ib.original.y0 - model.die.y0) / grid);
            if (ba != bb) return ba < bb;
            const int64_t aa = model.width(ia) * model.height(ia);
            const int64_t ab = model.width(ib) * model.height(ib);
            if (aa != ab) return aa > ab;
            return ia.input_order < ib.input_order;
        });
    } else {
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            const auto& ia = model.instances[a];
            const auto& ib = model.instances[b];
            const int64_t aa = model.width(ia) * model.height(ia);
            const int64_t ab = model.width(ib) * model.height(ib);
            if (aa != ab) return aa > ab;
            if (ia.original.x0 != ib.original.x0) return ia.original.x0 < ib.original.x0;
            return ia.input_order < ib.input_order;
        });
    }
    return order;
}

bool runPass(const PlacementModel& model, const std::vector<RowInterval>& intervals, int variant,
             double alpha, double threshold, PassState* state, std::string* error) {
    state->placements.assign(model.instances.size(), Point{0, 0});
    state->placed.assign(model.instances.size(), false);
    state->assigned.assign(intervals.size(), {});
    state->cell_interval.assign(model.instances.size(), -1);
    state->used_width.assign(intervals.size(), 0);
    const std::vector<size_t> order = makeOrder(model, variant, alpha, threshold);
    for (size_t id : order) {
        if (!insertCell(model, intervals, id, alpha, threshold, state, error)) {
            return false;
        }
    }
    return true;
}

void repair(const PlacementModel& model, const std::vector<RowInterval>& intervals, double alpha,
            double threshold, PassState* state) {
    ValidationResult current = validatePlacement(model, state->placements, intervals, alpha, threshold);
    if (!current.ok) {
        return;
    }
    for (int pass = 0; pass < kRepairPasses; ++pass) {
        std::vector<size_t> cells = model.cell_ids;
        std::sort(cells.begin(), cells.end(), [&](size_t a, size_t b) {
            const int64_t da = manhattan(originalPoint(model, a), state->placements[a]);
            const int64_t db = manhattan(originalPoint(model, b), state->placements[b]);
            if (da != db) return da > db;
            return model.instances[a].input_order < model.instances[b].input_order;
        });
        bool improved = false;
        const size_t limit = std::min<size_t>(kRepairCells, cells.size());
        for (size_t idx = 0; idx < limit; ++idx) {
            const size_t cell_id = cells[idx];
            const int old_interval = state->cell_interval[cell_id];
            if (old_interval < 0) {
                continue;
            }
            PassState trial = *state;
            auto& old_cells = trial.assigned[old_interval];
            old_cells.erase(std::remove(old_cells.begin(), old_cells.end(), cell_id), old_cells.end());
            trial.used_width[old_interval] -= model.width(model.instances[cell_id]);
            trial.placed[cell_id] = false;
            trial.cell_interval[cell_id] = -1;
            if (!old_cells.empty()) {
                std::vector<Point> solved_old = solveRowInterval(model, intervals[old_interval], old_cells);
                applySolvedRow(model, old_interval, intervals, solved_old, &trial);
            }
            std::string err;
            if (!insertCell(model, intervals, cell_id, alpha, threshold, &trial, &err)) {
                continue;
            }
            ValidationResult candidate =
                validatePlacement(model, trial.placements, intervals, alpha, threshold);
            if (!candidate.ok) {
                continue;
            }
            const bool accept_quality = candidate.metrics.flow_quality + 1e-9 < current.metrics.flow_quality;
            const bool accept_dor = alpha < 0.35 &&
                                    candidate.metrics.dor_percent + 1e-9 < current.metrics.dor_percent &&
                                    candidate.metrics.normalized_displacement <=
                                        current.metrics.normalized_displacement + 1.0;
            if (accept_quality || accept_dor) {
                *state = std::move(trial);
                current = candidate;
                improved = true;
            }
        }
        if (!improved) {
            break;
        }
    }
}

}  // namespace

std::vector<Point> solveRowInterval(const PlacementModel& model, const RowInterval& interval,
                                    const std::vector<size_t>& cell_ids) {
    std::vector<size_t> ids = cell_ids;
    std::sort(ids.begin(), ids.end(), [&](size_t a, size_t b) {
        const auto& ia = model.instances[a];
        const auto& ib = model.instances[b];
        if (ia.original.x0 != ib.original.x0) return ia.original.x0 < ib.original.x0;
        return ia.input_order < ib.input_order;
    });
    int64_t total_width = 0;
    for (size_t id : ids) {
        total_width += model.width(model.instances[id]);
    }
    if (total_width > interval.x1 - interval.x0) {
        throw PlacementError("row interval trial exceeds interval capacity");
    }

    std::vector<Cluster> clusters;
    for (size_t id : ids) {
        Cluster c;
        c.ids.push_back(id);
        c.width = model.width(model.instances[id]);
        c.weight = 1;
        c.q = static_cast<long double>(model.instances[id].original.x0);
        collapseCluster(model, interval, &c);
        clusters.push_back(c);
        while (clusters.size() >= 2) {
            Cluster& prev = clusters[clusters.size() - 2];
            Cluster& cur = clusters[clusters.size() - 1];
            if (prev.x + prev.width <= cur.x) {
                break;
            }
            Cluster merged = mergeClusters(interval, model, prev, cur);
            clusters.pop_back();
            clusters.pop_back();
            clusters.push_back(std::move(merged));
        }
    }

    std::vector<Point> placements(model.instances.size(), Point{0, 0});
    for (const Cluster& cluster : clusters) {
        int64_t x = cluster.x;
        for (size_t id : cluster.ids) {
            placements[id] = Point{x, interval.y};
            x += model.width(model.instances[id]);
        }
    }
    return placements;
}

LegalizationResult legalize(const PlacementModel& model, const std::vector<RowInterval>& intervals,
                            double alpha, double threshold) {
    if (model.cell_ids.empty()) {
        return LegalizationResult{std::vector<Point>(model.instances.size(), Point{0, 0}), Metrics{}};
    }
    if (intervals.empty()) {
        throw PlacementError("cannot legalize without legal row intervals");
    }

    bool have_best = false;
    LegalizationResult best;
    std::string last_error;
    for (int variant = 0; variant < 4; ++variant) {
        PassState state;
        std::string error;
        if (!runPass(model, intervals, variant, alpha, threshold, &state, &error)) {
            last_error = error;
            continue;
        }
        repair(model, intervals, alpha, threshold, &state);
        ValidationResult valid =
            validatePlacement(model, state.placements, intervals, alpha, threshold);
        if (!valid.ok) {
            std::ostringstream oss;
            oss << "variant " << variant << " failed validation: ";
            for (size_t i = 0; i < valid.errors.size() && i < 3; ++i) {
                if (i) oss << "; ";
                oss << valid.errors[i];
            }
            last_error = oss.str();
            continue;
        }
        if (!have_best || valid.metrics.flow_quality < best.metrics.flow_quality) {
            have_best = true;
            best.placements = std::move(state.placements);
            best.metrics = valid.metrics;
        }
    }
    if (!have_best) {
        throw PlacementError(last_error.empty() ? "legalization failed" : last_error);
    }
    return best;
}

}  // namespace legalizer
