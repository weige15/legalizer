#include "row_interval_builder.hpp"

#include <algorithm>

namespace legalizer {
namespace {

void normalizeIntervals(std::vector<RowInterval>& intervals) {
  std::vector<RowInterval> cleaned;
  for (const RowInterval& interval : intervals) {
    if (interval.x_min < interval.x_max) cleaned.push_back(interval);
  }
  std::sort(cleaned.begin(), cleaned.end(), [](const RowInterval& a,
                                               const RowInterval& b) {
    if (a.x_min != b.x_min) return a.x_min < b.x_min;
    return a.x_max < b.x_max;
  });

  intervals.clear();
  for (const RowInterval& interval : cleaned) {
    if (!intervals.empty() && intervals.back().x_max >= interval.x_min) {
      intervals.back().x_max = std::max(intervals.back().x_max, interval.x_max);
    } else {
      intervals.push_back(interval);
    }
  }
}

void subtractSpan(std::vector<RowInterval>& intervals, int64_t cut_min,
                  int64_t cut_max, const Design& design) {
  std::vector<RowInterval> next;
  for (const RowInterval& interval : intervals) {
    if (cut_max <= interval.x_min || interval.x_max <= cut_min) {
      next.push_back(interval);
      continue;
    }

    if (interval.x_min < cut_min) {
      RowInterval left{interval.x_min,
                       snapDown(cut_min, design.die.x_min, design.site_width)};
      if (left.x_min < left.x_max) next.push_back(left);
    }
    if (cut_max < interval.x_max) {
      RowInterval right{snapUp(cut_max, design.die.x_min, design.site_width),
                        interval.x_max};
      if (right.x_min < right.x_max) next.push_back(right);
    }
  }
  intervals = std::move(next);
  normalizeIntervals(intervals);
}

}  // namespace

bool intervalContains(const RowInterval& interval, int64_t x, int64_t width) {
  return interval.x_min <= x && x + width <= interval.x_max;
}

bool RowIntervalBuilder::build(const Design& design, std::vector<LegalRow>& rows,
                               std::string& error) {
  rows.clear();
  const int64_t die_width = rectWidth(design.die);
  const int64_t die_height = rectHeight(design.die);
  if (die_width <= 0 || die_height <= 0 || design.site_width <= 0 ||
      design.site_height <= 0) {
    error = "invalid die or site dimensions";
    return false;
  }

  const int64_t row_count = die_height / design.site_height;
  if (row_count <= 0) {
    error = "no complete legal site rows in die";
    return false;
  }

  const int64_t x_min = snapUp(design.die.x_min, design.die.x_min,
                               design.site_width);
  const int64_t x_max = snapDown(design.die.x_max, design.die.x_min,
                                 design.site_width);
  rows.reserve(static_cast<size_t>(row_count));
  for (int64_t i = 0; i < row_count; ++i) {
    LegalRow row;
    row.y = design.die.y_min + i * design.site_height;
    if (x_min < x_max) row.free_intervals.push_back(RowInterval{x_min, x_max});
    rows.push_back(std::move(row));
  }

  for (const Obstacle& obstacle : design.obstacles) {
    const Rect clipped = intersection(obstacle.rect, design.die);
    if (!isValidRect(clipped)) continue;

    int64_t first = floorDiv(clipped.y_min - design.die.y_min,
                             design.site_height);
    int64_t last = floorDiv(clipped.y_max - 1 - design.die.y_min,
                            design.site_height);
    first = clampInt64(first, 0, row_count - 1);
    last = clampInt64(last, 0, row_count - 1);

    for (int64_t r = first; r <= last; ++r) {
      const int64_t row_y = rows[static_cast<size_t>(r)].y;
      const Rect row_rect{design.die.x_min, row_y, design.die.x_max,
                          row_y + design.site_height};
      if (!intersects(clipped, row_rect)) continue;
      subtractSpan(rows[static_cast<size_t>(r)].free_intervals, clipped.x_min,
                   clipped.x_max, design);
    }
  }

  bool any = false;
  for (const LegalRow& row : rows) {
    if (!row.free_intervals.empty()) {
      any = true;
      break;
    }
  }
  if (!any) {
    error = "all site rows are blocked";
    return false;
  }
  return true;
}

}  // namespace legalizer
