#include "row_interval_builder.h"

#include <algorithm>
#include <stdexcept>

namespace legalizer {
namespace {

std::vector<Interval> mergeIntervals(std::vector<Interval> intervals) {
    std::vector<Interval> merged;
    std::sort(intervals.begin(), intervals.end(), [](const Interval& a, const Interval& b) {
        if (a.lx != b.lx) {
            return a.lx < b.lx;
        }
        return a.ux < b.ux;
    });
    for (const Interval& interval : intervals) {
        if (interval.lx >= interval.ux) {
            continue;
        }
        if (merged.empty() || merged.back().ux < interval.lx) {
            merged.push_back(interval);
        } else if (merged.back().ux < interval.ux) {
            merged.back().ux = interval.ux;
        }
    }
    return merged;
}

std::vector<Interval> subtractBlocked(const Interval& source, const std::vector<Interval>& blocked) {
    std::vector<Interval> free;
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
            free.push_back(Interval{cursor, blx});
        }
        if (cursor < bux) {
            cursor = bux;
        }
    }
    if (cursor < source.ux) {
        free.push_back(Interval{cursor, source.ux});
    }
    return free;
}

} // namespace

std::vector<Row> buildRowSegments(const Design& design) {
    validateDesignMetadata(design);

    std::vector<Row> rows;
    Coord usable_height = height(design.die);
    Coord row_count = usable_height / design.site_height;
    rows.reserve(static_cast<std::size_t>(row_count));

    for (Coord i = 0; i < row_count; ++i) {
        Coord row_y = design.die.ly + i * design.site_height;
        Rect row_rect{design.die.lx, row_y, design.die.ux, row_y + design.site_height};
        std::vector<Interval> blocked;

        for (const Obstacle& obstacle : design.obstacles) {
            Rect clipped = intersection(obstacle.rect, design.die);
            if (!isValid(clipped) || !overlaps(row_rect, clipped)) {
                continue;
            }
            blocked.push_back(Interval{clipped.lx, clipped.ux});
        }

        blocked = mergeIntervals(blocked);
        std::vector<Interval> free = subtractBlocked(Interval{design.die.lx, design.die.ux}, blocked);
        std::vector<Interval> snapped;
        for (const Interval& interval : free) {
            Coord lx = alignUp(interval.lx, design.die.lx, design.site_width);
            Coord ux = alignDown(interval.ux, design.die.lx, design.site_width);
            if (lx < ux) {
                snapped.push_back(Interval{lx, ux});
            }
        }
        rows.push_back(Row{row_y, static_cast<std::size_t>(i), snapped});
    }

    return rows;
}

bool intervalFits(const Interval& interval, Coord w) {
    return w > 0 && interval.lx + w <= interval.ux;
}

bool originFitsInInterval(const Interval& interval, Coord origin_x, Coord w) {
    return intervalFits(interval, w) && interval.lx <= origin_x && origin_x + w <= interval.ux;
}

} // namespace legalizer
