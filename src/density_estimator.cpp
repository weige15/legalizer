#include "density_estimator.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace legalizer {

namespace {

bool intervalContains(const RowInterval& interval, const Rect& rect) {
    return interval.y == rect.y0 && interval.x0 <= rect.x0 && rect.x1 <= interval.x1;
}

Rect binRect(const PlacementModel& model, int gx, int gy, int64_t grid) {
    int64_t x0 = model.die.x0 + static_cast<int64_t>(gx) * grid;
    int64_t y0 = model.die.y0 + static_cast<int64_t>(gy) * grid;
    return Rect{x0, y0, std::min(model.die.x1, x0 + grid), std::min(model.die.y1, y0 + grid)};
}

bool macroOverlaps(const PlacementModel& model, const Rect& rect) {
    for (size_t obs_id : model.obstacle_ids) {
        const auto& obs = model.instances[obs_id];
        if (obs.type == InstanceType::Macro && overlaps(rect, obs.original)) {
            return true;
        }
    }
    return false;
}

int64_t cellAreaInRect(const PlacementModel& model, const std::vector<Point>& placements,
                       const Rect& rect) {
    int64_t area = 0;
    for (size_t id : model.cell_ids) {
        if (id >= placements.size()) {
            continue;
        }
        area += overlapArea(model.rectAt(id, placements[id]), rect);
    }
    return area;
}

}  // namespace

DensitySummary computeDensitySummary(const PlacementModel& model, const std::vector<Point>& placements,
                                     double threshold) {
    const int64_t grid = 10 * model.dbu_per_micron;
    if (grid <= 0) {
        throw PlacementError("invalid density grid size");
    }
    const int cols = static_cast<int>((model.dieWidth() + grid - 1) / grid);
    const int rows = static_cast<int>((model.dieHeight() + grid - 1) / grid);

    DensitySummary summary;
    for (int gy = 0; gy < rows; ++gy) {
        for (int gx = 0; gx < cols; ++gx) {
            Rect bin = binRect(model, gx, gy, grid);
            const int64_t bin_area = (bin.x1 - bin.x0) * (bin.y1 - bin.y0);
            if (bin_area <= 0 || macroOverlaps(model, bin)) {
                continue;
            }
            ++summary.total_bins;
            const double density =
                100.0 * static_cast<double>(cellAreaInRect(model, placements, bin)) /
                static_cast<double>(bin_area);
            if (density > threshold) {
                ++summary.overflow_bins;
            }
        }
    }
    if (summary.total_bins > 0) {
        summary.dor_percent =
            100.0 * static_cast<double>(summary.overflow_bins) / summary.total_bins;
    }
    return summary;
}

ValidationResult validatePlacement(const PlacementModel& model, const std::vector<Point>& placements,
                                   const std::vector<RowInterval>& intervals, double alpha,
                                   double threshold) {
    ValidationResult result;
    std::map<int, std::vector<std::pair<int64_t, Rect>>> by_row;
    double total_disp_dbu = 0.0;

    for (size_t id : model.cell_ids) {
        if (id >= placements.size()) {
            result.errors.push_back("missing placement for " + model.instances[id].name);
            continue;
        }
        const Instance& inst = model.instances[id];
        const Point ll = placements[id];
        const Rect placed = model.rectAt(id, ll);
        const Point orig{inst.original.x0, inst.original.y0};
        total_disp_dbu += static_cast<double>(manhattan(orig, ll));

        if (!model.isSingleRowCell(id)) {
            result.errors.push_back("unsupported multi-row cell " + inst.name);
        }
        if (!contains(model.die, placed)) {
            result.errors.push_back("cell outside die: " + inst.name);
        }
        if (!model.isSiteAlignedX(ll.x)) {
            result.errors.push_back("cell off site grid: " + inst.name);
        }
        const int row = model.rowIndexForY(ll.y);
        if (row < 0) {
            result.errors.push_back("cell off legal row: " + inst.name);
        } else {
            by_row[row].push_back({placed.x0, placed});
            bool in_interval = false;
            for (const auto& interval : intervals) {
                if (interval.row_index == row && intervalContains(interval, placed)) {
                    in_interval = true;
                    break;
                }
            }
            if (!in_interval) {
                result.errors.push_back("cell not contained in legal interval: " + inst.name);
            }
        }
        for (size_t obs_id : model.obstacle_ids) {
            if (overlaps(placed, model.instances[obs_id].original)) {
                result.errors.push_back("cell overlaps obstacle: " + inst.name + " vs " +
                                        model.instances[obs_id].name);
            }
        }
    }

    for (auto& kv : by_row) {
        auto& rects = kv.second;
        std::sort(rects.begin(), rects.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (size_t i = 1; i < rects.size(); ++i) {
            if (overlaps(rects[i - 1].second, rects[i].second)) {
                result.errors.push_back("cell-cell overlap on row " + std::to_string(kv.first));
            }
        }
    }

    const double count = static_cast<double>(std::max<size_t>(1, model.cell_ids.size()));
    result.metrics.avg_displacement_um =
        (total_disp_dbu / static_cast<double>(model.dbu_per_micron)) / count;
    result.metrics.normalized_displacement = result.metrics.avg_displacement_um * 18.2;
    DensitySummary density = computeDensitySummary(model, placements, threshold);
    result.metrics.dor_percent = density.dor_percent;
    result.metrics.flow_quality =
        alpha * result.metrics.normalized_displacement + (1.0 - alpha) * density.dor_percent;
    result.metrics.handout_quality =
        alpha * result.metrics.avg_displacement_um + (1.0 - alpha) * density.dor_percent;
    result.ok = result.errors.empty();
    return result;
}

double estimateLocalDensityPenalty(const PlacementModel& model,
                                   const std::vector<Point>& placements,
                                   const std::vector<bool>& placed,
                                   size_t trial_cell_id,
                                   Point trial_ll,
                                   double threshold) {
    const int64_t grid = 10 * model.dbu_per_micron;
    const Rect trial = model.rectAt(trial_cell_id, trial_ll);
    const int gx0 = static_cast<int>((std::max(trial.x0, model.die.x0) - model.die.x0) / grid);
    const int gy0 = static_cast<int>((std::max(trial.y0, model.die.y0) - model.die.y0) / grid);
    const int gx1 = static_cast<int>((std::max<int64_t>(0, trial.x1 - model.die.x0 - 1) / grid));
    const int gy1 = static_cast<int>((std::max<int64_t>(0, trial.y1 - model.die.y0 - 1) / grid));
    double penalty = 0.0;
    int bins = 0;
    for (int gy = gy0; gy <= gy1; ++gy) {
        for (int gx = gx0; gx <= gx1; ++gx) {
            Rect bin = binRect(model, gx, gy, grid);
            const int64_t bin_area = (bin.x1 - bin.x0) * (bin.y1 - bin.y0);
            if (bin_area <= 0 || macroOverlaps(model, bin)) {
                continue;
            }
            int64_t area = overlapArea(trial, bin);
            for (size_t id : model.cell_ids) {
                if (id < placed.size() && placed[id] && id < placements.size()) {
                    area += overlapArea(model.rectAt(id, placements[id]), bin);
                }
            }
            const double density = 100.0 * static_cast<double>(area) / bin_area;
            penalty += density > threshold ? (density - threshold) + 100.0 : density * 0.1;
            ++bins;
        }
    }
    return bins > 0 ? penalty / bins : 0.0;
}

}  // namespace legalizer
