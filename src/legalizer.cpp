#include "legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace legalizer {
namespace {

struct Cluster {
    std::vector<RowCell> cells;
    Coord width = 0;
    long double q = 0.0L;
    Coord x = 0;
};

Coord nearestAlignedInRange(Coord target, Coord min_x, Coord max_x, Coord origin, Coord step) {
    if (min_x > max_x) {
        return min_x;
    }
    Coord down = alignDown(target, origin, step);
    Coord up = alignUp(target, origin, step);
    Coord best = min_x;
    bool have = false;
    auto consider = [&](Coord x) {
        if (x < min_x || x > max_x) {
            return;
        }
        if (!have || std::llabs(x - target) < std::llabs(best - target) ||
            (std::llabs(x - target) == std::llabs(best - target) && x < best)) {
            best = x;
            have = true;
        }
    };
    consider(down);
    consider(up);
    consider(alignUp(min_x, origin, step));
    consider(alignDown(max_x, origin, step));
    if (!have) {
        best = alignUp(min_x, origin, step);
    }
    return best;
}

void recomputeCluster(Cluster& cluster, const Interval& segment, Coord site_origin, Coord site_width) {
    Coord offset = 0;
    cluster.q = 0.0L;
    for (const RowCell& cell : cluster.cells) {
        cluster.q += static_cast<long double>(cell.original_x - offset);
        offset += cell.width;
    }
    cluster.width = offset;
    Coord min_x = segment.lx;
    Coord max_x = segment.ux - cluster.width;
    Coord target = static_cast<Coord>(std::llround(cluster.q / static_cast<long double>(cluster.cells.size())));
    cluster.x = nearestAlignedInRange(target, min_x, max_x, site_origin, site_width);
}

std::vector<Interval> mergeIntervalsLocal(std::vector<Interval> intervals) {
    std::sort(intervals.begin(), intervals.end(), [](const Interval& a, const Interval& b) {
        if (a.lx != b.lx) {
            return a.lx < b.lx;
        }
        return a.ux < b.ux;
    });
    std::vector<Interval> out;
    for (const Interval& interval : intervals) {
        if (interval.lx >= interval.ux) {
            continue;
        }
        if (out.empty() || out.back().ux < interval.lx) {
            out.push_back(interval);
        } else {
            out.back().ux = std::max(out.back().ux, interval.ux);
        }
    }
    return out;
}

std::vector<Interval> subtractIntervals(std::vector<Interval> free, std::vector<Interval> blocked) {
    blocked = mergeIntervalsLocal(blocked);
    std::vector<Interval> result;
    for (const Interval& source : free) {
        Coord cursor = source.lx;
        for (const Interval& block : blocked) {
            if (block.ux <= cursor) {
                continue;
            }
            if (source.ux <= block.lx) {
                break;
            }
            Coord blx = std::max(source.lx, block.lx);
            Coord bux = std::min(source.ux, block.ux);
            if (cursor < blx) {
                result.push_back(Interval{cursor, blx});
            }
            cursor = std::max(cursor, bux);
        }
        if (cursor < source.ux) {
            result.push_back(Interval{cursor, source.ux});
        }
    }
    return result;
}

std::vector<Interval> intersectIntervals(const std::vector<Interval>& a, const std::vector<Interval>& b) {
    std::vector<Interval> out;
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() && j < b.size()) {
        Coord lx = std::max(a[i].lx, b[j].lx);
        Coord ux = std::min(a[i].ux, b[j].ux);
        if (lx < ux) {
            out.push_back(Interval{lx, ux});
        }
        if (a[i].ux < b[j].ux) {
            ++i;
        } else {
            ++j;
        }
    }
    return out;
}

struct TrialCellPlacement {
    std::size_t index = 0;
    Coord lx = 0;
    Coord ly = 0;
};

struct Candidate {
    bool valid = false;
    Coord lx = 0;
    Coord ly = 0;
    double score = std::numeric_limits<double>::infinity();
    Coord displacement = 0;
};

struct TrialState {
    std::vector<std::optional<Placement>> placements;
    std::vector<Rect> placed_rects;
    Coord total_displacement = 0;
    DensityEstimator density;

    TrialState(const Design& design)
        : placements(design.cells.size()), density(design) {}
};

bool verticalOverlaps(Coord ly, Coord uy, const Rect& rect) {
    return ly < rect.uy && rect.ly < uy;
}

std::vector<Interval> snapIntervals(const std::vector<Interval>& intervals,
                                    Coord die_lx,
                                    Coord site_width) {
    std::vector<Interval> out;
    for (const Interval& interval : intervals) {
        Coord lx = alignUp(interval.lx, die_lx, site_width);
        Coord ux = alignDown(interval.ux, die_lx, site_width);
        if (lx < ux) {
            out.push_back(Interval{lx, ux});
        }
    }
    return out;
}

std::vector<std::size_t> rowOrderByDistance(const Design& design,
                                            const std::vector<Row>& rows,
                                            const MovableCell& cell,
                                            Coord row_span) {
    std::vector<std::size_t> starts;
    if (row_span <= 0 || rows.size() < static_cast<std::size_t>(row_span)) {
        return starts;
    }
    std::size_t last = rows.size() - static_cast<std::size_t>(row_span);
    for (std::size_t i = 0; i <= last; ++i) {
        Coord y = rows[i].y;
        if (y + cell.height <= design.die.uy) {
            starts.push_back(i);
        }
    }
    std::sort(starts.begin(), starts.end(), [&](std::size_t a, std::size_t b) {
        Coord da = std::llabs(rows[a].y - cell.original_ly);
        Coord db = std::llabs(rows[b].y - cell.original_ly);
        if (da != db) {
            return da < db;
        }
        return rows[a].y < rows[b].y;
    });
    return starts;
}

std::vector<Interval> freeIntervalsForSpan(const Design& design,
                                           const std::vector<Row>& rows,
                                           const TrialState& state,
                                           std::size_t row_start,
                                           Coord row_span,
                                           Coord cell_ly,
                                           Coord cell_height) {
    std::vector<Interval> common = rows[row_start].segments;
    for (Coord offset = 1; offset < row_span; ++offset) {
        common = intersectIntervals(common, rows[row_start + static_cast<std::size_t>(offset)].segments);
        if (common.empty()) {
            return common;
        }
    }

    Coord cell_uy = cell_ly + cell_height;
    std::vector<Interval> blocked;
    for (const Rect& placed : state.placed_rects) {
        if (verticalOverlaps(cell_ly, cell_uy, placed)) {
            blocked.push_back(Interval{placed.lx, placed.ux});
        }
    }
    common = subtractIntervals(common, blocked);
    return snapIntervals(common, design.die.lx, design.site_width);
}

Candidate findCandidate(const Design& design,
                        const std::vector<Row>& rows,
                        const TrialState& state,
                        std::size_t cell_index,
                        const LegalizerConfig& config) {
    const MovableCell& cell = design.cells[cell_index];
    Coord row_span = std::max<Coord>(1, ceilDiv(cell.height, design.site_height));
    std::vector<std::size_t> starts = rowOrderByDistance(design, rows, cell, row_span);
    Candidate best;

    for (std::size_t row_start : starts) {
        Coord ly = rows[row_start].y;
        std::vector<Interval> intervals = freeIntervalsForSpan(design, rows, state, row_start,
                                                               row_span, ly, cell.height);
        for (const Interval& interval : intervals) {
            if (!intervalFits(interval, cell.width)) {
                continue;
            }
            Coord min_x = interval.lx;
            Coord max_x = interval.ux - cell.width;
            std::vector<Coord> xs;
            xs.push_back(nearestAlignedInRange(cell.original_lx, min_x, max_x,
                                               design.die.lx, design.site_width));
            xs.push_back(alignUp(min_x, design.die.lx, design.site_width));
            xs.push_back(alignDown(max_x, design.die.lx, design.site_width));
            std::sort(xs.begin(), xs.end());
            xs.erase(std::unique(xs.begin(), xs.end()), xs.end());

            for (Coord lx : xs) {
                if (lx < min_x || lx > max_x || !isSiteAligned(lx, design.die.lx, design.site_width)) {
                    continue;
                }
                Rect rect = rectFromOrigin(cell, lx, ly);
                if (!contains(design.die, rect)) {
                    continue;
                }
                Coord disp = manhattanDisplacementDbu(cell, lx, ly);
                double disp_u = static_cast<double>(disp) / static_cast<double>(design.dbu_per_micron);
                double dor = state.density.dorWithAddedRect(rect, config.threshold);
                double score = config.alpha * disp_u * 18.2 + (1.0 - config.alpha) * dor;
                score += 1e-9 * static_cast<double>(cell.input_index);
                if (!best.valid || score < best.score ||
                    (score == best.score && disp < best.displacement) ||
                    (score == best.score && disp == best.displacement &&
                     (ly < best.ly || (ly == best.ly && lx < best.lx)))) {
                    best.valid = true;
                    best.lx = lx;
                    best.ly = ly;
                    best.score = score;
                    best.displacement = disp;
                }
            }
        }
    }
    return best;
}

std::vector<std::size_t> makeOrder(const Design& design, int mode) {
    std::vector<std::size_t> order(design.cells.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    if (mode == 0) {
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            const MovableCell& ca = design.cells[a];
            const MovableCell& cb = design.cells[b];
            if (ca.original_lx != cb.original_lx) {
                return ca.original_lx < cb.original_lx;
            }
            if (ca.original_ly != cb.original_ly) {
                return ca.original_ly < cb.original_ly;
            }
            return ca.input_index < cb.input_index;
        });
    } else if (mode == 1) {
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            const MovableCell& ca = design.cells[a];
            const MovableCell& cb = design.cells[b];
            if (ca.original_lx != cb.original_lx) {
                return ca.original_lx > cb.original_lx;
            }
            if (ca.original_ly != cb.original_ly) {
                return ca.original_ly < cb.original_ly;
            }
            return ca.input_index < cb.input_index;
        });
    } else {
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            const MovableCell& ca = design.cells[a];
            const MovableCell& cb = design.cells[b];
            Coord spana = ceilDiv(ca.height, design.site_height);
            Coord spanb = ceilDiv(cb.height, design.site_height);
            bool talla = spana > 1;
            bool tallb = spanb > 1;
            if (talla != tallb) {
                return talla;
            }
            Coord aa = ca.width * ca.height;
            Coord ab = cb.width * cb.height;
            if (aa != ab) {
                return aa > ab;
            }
            if (ca.original_lx != cb.original_lx) {
                return ca.original_lx < cb.original_lx;
            }
            return ca.input_index < cb.input_index;
        });
    }
    return order;
}

double finalTrialScore(const Design& design, const TrialState& state, const LegalizerConfig& config) {
    double avg_disp_u = 0.0;
    if (!design.cells.empty()) {
        avg_disp_u = static_cast<double>(state.total_displacement) /
                     static_cast<double>(design.dbu_per_micron) /
                     static_cast<double>(design.cells.size());
    }
    return config.alpha * avg_disp_u * 18.2 +
           (1.0 - config.alpha) * state.density.dor(config.threshold);
}

bool runTrial(const Design& design,
              const std::vector<Row>& rows,
              const LegalizerConfig& config,
              int mode,
              TrialState& state,
              std::string& error) {
    std::vector<std::size_t> order = makeOrder(design, mode);
    for (std::size_t idx : order) {
        Candidate candidate = findCandidate(design, rows, state, idx, config);
        if (!candidate.valid) {
            error = "no legal location found for cell '" + design.cells[idx].name + "'";
            return false;
        }
        Placement placement{candidate.lx, candidate.ly};
        state.placements[idx] = placement;
        Rect rect = rectFromOrigin(design.cells[idx], placement.lx, placement.ly);
        state.placed_rects.push_back(rect);
        state.total_displacement += manhattanDisplacementDbu(design.cells[idx],
                                                             placement.lx,
                                                             placement.ly);
        state.density.addRect(rect);
    }
    return true;
}

} // namespace

RowPlacementResult placeRowAbacus(const std::vector<RowCell>& input_cells,
                                  const Interval& segment,
                                  Coord site_origin,
                                  Coord site_width) {
    RowPlacementResult result;
    if (site_width <= 0 || segment.lx >= segment.ux) {
        return result;
    }

    std::vector<RowCell> cells = input_cells;
    std::sort(cells.begin(), cells.end(), [](const RowCell& a, const RowCell& b) {
        if (a.original_x != b.original_x) {
            return a.original_x < b.original_x;
        }
        return a.input_index < b.input_index;
    });

    Coord total_width = 0;
    for (const RowCell& cell : cells) {
        if (cell.width <= 0) {
            return result;
        }
        total_width += cell.width;
    }
    if (total_width > width(Rect{segment.lx, 0, segment.ux, 1})) {
        return result;
    }

    std::vector<Cluster> clusters;
    for (const RowCell& cell : cells) {
        Cluster cluster;
        cluster.cells.push_back(cell);
        recomputeCluster(cluster, segment, site_origin, site_width);
        clusters.push_back(cluster);

        while (clusters.size() >= 2) {
            Cluster& prev = clusters[clusters.size() - 2];
            Cluster& cur = clusters[clusters.size() - 1];
            if (prev.x + prev.width <= cur.x) {
                break;
            }
            prev.cells.insert(prev.cells.end(), cur.cells.begin(), cur.cells.end());
            recomputeCluster(prev, segment, site_origin, site_width);
            clusters.pop_back();
        }
    }

    for (const Cluster& cluster : clusters) {
        Coord x = cluster.x;
        for (const RowCell& cell : cluster.cells) {
            if (!originFitsInInterval(segment, x, cell.width) ||
                !isSiteAligned(x, site_origin, site_width)) {
                result.origins.clear();
                return result;
            }
            result.origins[cell.cell_index] = x;
            x += cell.width;
        }
    }
    result.feasible = result.origins.size() == cells.size();
    return result;
}

void legalizeDesign(Design& design,
                    const std::vector<Row>& rows,
                    const LegalizerConfig& config) {
    validateDesignMetadata(design);
    if (rows.empty() && !design.cells.empty()) {
        throw std::runtime_error("design has no legal rows");
    }

    bool have_best = false;
    double best_score = std::numeric_limits<double>::infinity();
    std::vector<std::optional<Placement>> best_placements;
    std::string last_error;

    for (int mode = 0; mode < 3; ++mode) {
        TrialState state(design);
        std::string error;
        if (!runTrial(design, rows, config, mode, state, error)) {
            last_error = error;
            continue;
        }
        double score = finalTrialScore(design, state, config);
        if (!have_best || score < best_score) {
            have_best = true;
            best_score = score;
            best_placements = state.placements;
        }
    }

    if (!have_best) {
        throw std::runtime_error(last_error.empty() ? "legalization failed" : last_error);
    }

    for (std::size_t i = 0; i < design.cells.size(); ++i) {
        design.cells[i].legal = best_placements[i];
    }
}

bool checkLegality(const Design& design,
                   const std::vector<Row>& rows,
                   std::string& error) {
    validateDesignMetadata(design);
    std::set<Coord> row_y;
    for (const Row& row : rows) {
        row_y.insert(row.y);
    }

    std::vector<Rect> rects;
    rects.reserve(design.cells.size());
    for (const MovableCell& cell : design.cells) {
        if (!cell.legal.has_value()) {
            error = "cell '" + cell.name + "' has no legal placement";
            return false;
        }
        Rect rect = placedRect(cell);
        if (!contains(design.die, rect)) {
            error = "cell '" + cell.name + "' is outside the die";
            return false;
        }
        if (!isSiteAligned(cell.legal->lx, design.die.lx, design.site_width)) {
            error = "cell '" + cell.name + "' is not site-aligned in X";
            return false;
        }
        if (row_y.find(cell.legal->ly) == row_y.end()) {
            error = "cell '" + cell.name + "' is not on a legal row";
            return false;
        }
        for (const Obstacle& obstacle : design.obstacles) {
            if (overlaps(rect, obstacle.rect)) {
                error = "cell '" + cell.name + "' overlaps fixed obstacle '" + obstacle.name + "'";
                return false;
            }
        }
        rects.push_back(rect);
    }

    std::vector<std::size_t> order(rects.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (rects[a].lx != rects[b].lx) {
            return rects[a].lx < rects[b].lx;
        }
        return design.cells[a].input_index < design.cells[b].input_index;
    });

    std::vector<std::size_t> active;
    for (std::size_t idx : order) {
        active.erase(std::remove_if(active.begin(), active.end(), [&](std::size_t other) {
            return rects[other].ux <= rects[idx].lx;
        }), active.end());
        for (std::size_t other : active) {
            if (overlaps(rects[idx], rects[other])) {
                error = "cell '" + design.cells[idx].name + "' overlaps cell '" +
                        design.cells[other].name + "'";
                return false;
            }
        }
        active.push_back(idx);
    }
    return true;
}

} // namespace legalizer
