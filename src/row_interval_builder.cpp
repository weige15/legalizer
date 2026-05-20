#include "row_interval_builder.h"

#include <algorithm>
#include <sstream>

namespace legalizer {

namespace {

struct Span {
    int64_t x0 = 0;
    int64_t x1 = 0;
};

void subtractSpan(std::vector<Span>* spans, int64_t cut0, int64_t cut1) {
    if (cut0 >= cut1) {
        return;
    }
    std::vector<Span> next;
    for (const auto& span : *spans) {
        if (cut1 <= span.x0 || cut0 >= span.x1) {
            next.push_back(span);
            continue;
        }
        if (span.x0 < cut0) {
            next.push_back(Span{span.x0, std::min(cut0, span.x1)});
        }
        if (cut1 < span.x1) {
            next.push_back(Span{std::max(cut1, span.x0), span.x1});
        }
    }
    *spans = next;
}

}  // namespace

std::vector<RowInterval> buildRowIntervals(const PlacementModel& model) {
    model.validateBasic();
    int64_t total_cell_width = 0;
    int64_t min_cell_width = 0;
    for (size_t id : model.cell_ids) {
        const auto& inst = model.instances[id];
        if (!model.isSingleRowCell(id)) {
            throw PlacementError("unsupported movable cell height for " + inst.name +
                                 ": expected one site row");
        }
        total_cell_width += model.width(inst);
        min_cell_width = min_cell_width == 0 ? model.width(inst)
                                             : std::min(min_cell_width, model.width(inst));
    }

    std::vector<RowInterval> intervals;
    int64_t total_capacity = 0;
    const int rows = model.rowCount();
    for (int row = 0; row < rows; ++row) {
        const int64_t y = model.rowY(row);
        Rect row_rect{model.die.x0, y, model.die.x1, y + model.site_height};
        std::vector<Span> spans{Span{model.die.x0, model.die.x1}};

        for (size_t obs_id : model.obstacle_ids) {
            const Rect& obs = model.instances[obs_id].original;
            if (!overlaps(row_rect, obs)) {
                continue;
            }
            subtractSpan(&spans, std::max(model.die.x0, obs.x0), std::min(model.die.x1, obs.x1));
        }

        std::sort(spans.begin(), spans.end(), [](const Span& a, const Span& b) {
            if (a.x0 != b.x0) return a.x0 < b.x0;
            return a.x1 < b.x1;
        });

        for (const auto& span : spans) {
            int64_t x0 = snapUpToGrid(span.x0, model.die.x0, model.site_width);
            int64_t x1 = snapDownToGrid(span.x1, model.die.x0, model.site_width);
            if (x1 > x0 && (model.cell_ids.empty() || x1 - x0 >= min_cell_width)) {
                intervals.push_back(RowInterval{row, y, x0, x1});
                total_capacity += x1 - x0;
            }
        }
    }

    if (!model.cell_ids.empty() && intervals.empty()) {
        throw PlacementError("no legal row intervals after subtracting obstacles");
    }
    if (total_capacity < total_cell_width) {
        std::ostringstream oss;
        oss << "insufficient legal row capacity: need " << total_cell_width << " DBU, have "
            << total_capacity << " DBU";
        throw PlacementError(oss.str());
    }
    return intervals;
}

}  // namespace legalizer
