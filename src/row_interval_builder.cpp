#include "row_interval_builder.h"

#include <algorithm>

namespace legalizer {
namespace {

void subtractSpan(std::vector<RowInterval> *intervals, Coord block_llx, Coord block_urx) {
  std::vector<RowInterval> next;
  for (const RowInterval &interval : *intervals) {
    if (block_urx <= interval.llx || interval.urx <= block_llx) {
      next.push_back(interval);
      continue;
    }
    if (interval.llx < block_llx) {
      next.push_back(RowInterval{interval.llx, std::min(interval.urx, block_llx)});
    }
    if (block_urx < interval.urx) {
      next.push_back(RowInterval{std::max(interval.llx, block_urx), interval.urx});
    }
  }
  *intervals = std::move(next);
}

std::vector<RowInterval> snapIntervals(const PlacementModel &model,
                                       const std::vector<RowInterval> &intervals) {
  std::vector<RowInterval> snapped;
  for (const RowInterval &interval : intervals) {
    Coord llx = alignUp(interval.llx, model.die.llx, model.site_width);
    Coord urx = alignDown(interval.urx, model.die.llx, model.site_width);
    if (llx < urx) {
      snapped.push_back(RowInterval{llx, urx});
    }
  }
  return snapped;
}

}  // namespace

RowBuildResult buildRowIntervals(const PlacementModel &model) {
  RowBuildResult result;
  if (!isValid(model.die) || model.site_width <= 0 || model.site_height <= 0) {
    result.error = "invalid die or site dimensions";
    return result;
  }

  Coord row_count_coord = height(model.die) / model.site_height;
  if (row_count_coord <= 0) {
    result.error = "die does not contain a full site row";
    return result;
  }

  bool has_legal_interval = false;
  result.rows.reserve(static_cast<std::size_t>(row_count_coord));
  for (Coord i = 0; i < row_count_coord; ++i) {
    SiteRow row;
    row.index = static_cast<int>(i);
    row.y = model.die.lly + i * model.site_height;
    row.intervals.push_back(RowInterval{model.die.llx, model.die.urx});

    Rect row_rect{model.die.llx, row.y, model.die.urx, row.y + model.site_height};
    for (const Obstacle &obstacle : model.obstacles) {
      Rect clipped = intersection(row_rect, obstacle.rect);
      if (!isValid(clipped)) {
        continue;
      }
      subtractSpan(&row.intervals, clipped.llx, clipped.urx);
    }
    row.intervals = snapIntervals(model, row.intervals);
    if (!row.intervals.empty()) {
      has_legal_interval = true;
    }
    result.rows.push_back(std::move(row));
  }

  if (!has_legal_interval) {
    result.error = "no legal row interval remains after subtracting obstacles";
    return result;
  }
  result.ok = true;
  return result;
}

}  // namespace legalizer
