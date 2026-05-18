#include "row_model.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace {
bool intervalLess(const Interval &a, const Interval &b) {
  return a.l < b.l || (a.l == b.l && a.r < b.r);
}
} // namespace

RowModel::RowModel(const Design &design) : design_(design) {
  const Coord numRows = design_.die.height() / design_.siteHeight;
  if (numRows <= 0) {
    throw std::runtime_error("die has no legal placement rows");
  }
  rows_.reserve(static_cast<std::size_t>(numRows));
  for (Coord i = 0; i < numRows; ++i) {
    rows_.push_back(Row{design_.die.lly + i * design_.siteHeight,
                        std::vector<Interval>{{design_.die.llx, design_.die.urx}}, {}});
  }

  for (const Obstacle &obs : design_.obstacles) {
    const int first = std::max(0, static_cast<int>(floorDiv(obs.rect.lly - design_.die.lly, design_.siteHeight)));
    const int last = std::min(rowCount() - 1,
                              static_cast<int>(floorDiv(obs.rect.ury - 1 - design_.die.lly, design_.siteHeight)));
    for (int r = first; r <= last; ++r) {
      const Rect band{design_.die.llx, rows_[r].y, design_.die.urx, rows_[r].y + design_.siteHeight};
      if (overlaps(band, obs.rect)) {
        rows_[r].free = subtractInterval(rows_[r].free, Interval{obs.rect.llx, obs.rect.urx});
      }
    }
  }
}

int RowModel::rowIndexForY(Coord y) const {
  if (!aligned(y, design_.die.lly, design_.siteHeight)) {
    return -1;
  }
  const Coord idx = (y - design_.die.lly) / design_.siteHeight;
  if (idx < 0 || idx >= rowCount()) {
    return -1;
  }
  return static_cast<int>(idx);
}

int RowModel::nearestRow(Coord y) const {
  Coord snapped = snapDown(y, design_.die.lly, design_.siteHeight);
  if (snapped < design_.die.lly) {
    snapped = design_.die.lly;
  }
  if (snapped > rows_.back().y) {
    snapped = rows_.back().y;
  }
  return rowIndexForY(snapped);
}

int RowModel::rowsNeeded(const Cell &cell) const {
  return static_cast<int>(ceilDiv(cell.original.height(), design_.siteHeight));
}

bool RowModel::legalSite(Coord x, Coord y) const {
  return aligned(x, design_.die.llx, design_.siteWidth) && aligned(y, design_.die.lly, design_.siteHeight);
}

bool RowModel::intervalContained(const std::vector<Interval> &intervals, Interval q) const {
  if (q.l >= q.r) {
    return false;
  }
  for (const Interval &iv : intervals) {
    if (iv.l <= q.l && q.r <= iv.r) {
      return true;
    }
    if (iv.l > q.l) {
      return false;
    }
  }
  return false;
}

bool RowModel::baseCanPlace(const Cell &cell, Coord x, Coord y) const {
  if (!legalSite(x, y)) {
    return false;
  }
  const Rect pr = placedRect(cell.original, x, y);
  if (!contains(design_.die, pr)) {
    return false;
  }
  const int start = rowIndexForY(y);
  if (start < 0) {
    return false;
  }
  const int need = rowsNeeded(cell);
  if (start + need > rowCount()) {
    return false;
  }
  const Interval span{x, x + cell.original.width()};
  for (int r = start; r < start + need; ++r) {
    if (!intervalContained(rows_[r].free, span)) {
      return false;
    }
  }
  return true;
}

bool RowModel::canPlace(const Cell &cell, Coord x, Coord y) const {
  if (!baseCanPlace(cell, x, y)) {
    return false;
  }
  const int start = rowIndexForY(y);
  const int need = rowsNeeded(cell);
  const Interval span{x, x + cell.original.width()};
  for (int r = start; r < start + need; ++r) {
    const auto &occ = rows_[r].occupied;
    auto it = std::lower_bound(occ.begin(), occ.end(), Interval{span.l, span.l}, intervalLess);
    if (it != occ.end() && intervalOverlaps(*it, span)) {
      return false;
    }
    if (it != occ.begin()) {
      --it;
      if (intervalOverlaps(*it, span)) {
        return false;
      }
    }
  }
  return true;
}

bool RowModel::commit(const Cell &cell, Coord x, Coord y) {
  if (!canPlace(cell, x, y)) {
    return false;
  }
  const int start = rowIndexForY(y);
  const int need = rowsNeeded(cell);
  const Interval span{x, x + cell.original.width()};
  for (int r = start; r < start + need; ++r) {
    auto &occ = rows_[r].occupied;
    occ.insert(std::lower_bound(occ.begin(), occ.end(), span, intervalLess), span);
  }
  return true;
}

bool RowModel::uncommit(const Cell &cell, Coord x, Coord y) {
  const int start = rowIndexForY(y);
  if (start < 0) {
    return false;
  }
  const int need = rowsNeeded(cell);
  if (start + need > rowCount()) {
    return false;
  }
  const Interval span{x, x + cell.original.width()};
  for (int r = start; r < start + need; ++r) {
    auto &occ = rows_[r].occupied;
    auto it = std::find_if(occ.begin(), occ.end(), [&](const Interval &iv) {
      return iv.l == span.l && iv.r == span.r;
    });
    if (it == occ.end()) {
      return false;
    }
  }
  for (int r = start; r < start + need; ++r) {
    auto &occ = rows_[r].occupied;
    auto it = std::find_if(occ.begin(), occ.end(), [&](const Interval &iv) {
      return iv.l == span.l && iv.r == span.r;
    });
    occ.erase(it);
  }
  return true;
}

std::vector<Interval> RowModel::availableSegments(const Cell &cell, int row) const {
  const int need = rowsNeeded(cell);
  if (row < 0 || row + need > rowCount()) {
    return {};
  }
  std::vector<Interval> segs = rows_[row].free;
  for (int r = row + 1; r < row + need; ++r) {
    segs = intersectIntervals(segs, rows_[r].free);
  }
  for (int r = row; r < row + need; ++r) {
    for (const Interval &occ : rows_[r].occupied) {
      segs = subtractInterval(segs, occ);
      if (segs.empty()) {
        return {};
      }
    }
  }
  return segs;
}

std::vector<Coord> RowModel::candidateXs(const Cell &cell, int row, Coord targetX,
                                         int perSegmentRadius) const {
  std::set<Coord> out;
  const Coord w = cell.original.width();
  for (const Interval &seg : availableSegments(cell, row)) {
    const Coord minX = snapUp(seg.l, design_.die.llx, design_.siteWidth);
    const Coord maxX = snapDown(seg.r - w, design_.die.llx, design_.siteWidth);
    if (minX > maxX) {
      continue;
    }
    Coord center = snapDown(std::clamp(targetX, minX, maxX), design_.die.llx, design_.siteWidth);
    center = std::clamp(center, minX, maxX);
    out.insert(minX);
    out.insert(maxX);
    out.insert(center);
    for (int k = 1; k <= perSegmentRadius; ++k) {
      const Coord left = center - k * design_.siteWidth;
      const Coord right = center + k * design_.siteWidth;
      if (left >= minX) {
        out.insert(left);
      }
      if (right <= maxX) {
        out.insert(right);
      }
    }
  }
  return std::vector<Coord>(out.begin(), out.end());
}

std::vector<Interval> RowModel::normalize(std::vector<Interval> intervals) {
  std::sort(intervals.begin(), intervals.end(), intervalLess);
  std::vector<Interval> out;
  for (const Interval &iv : intervals) {
    if (iv.l >= iv.r) {
      continue;
    }
    if (out.empty() || out.back().r < iv.l) {
      out.push_back(iv);
    } else {
      out.back().r = std::max(out.back().r, iv.r);
    }
  }
  return out;
}

std::vector<Interval> RowModel::subtractInterval(const std::vector<Interval> &intervals, Interval cut) {
  std::vector<Interval> out;
  for (const Interval &iv : intervals) {
    if (!intervalOverlaps(iv, cut)) {
      out.push_back(iv);
      continue;
    }
    if (iv.l < cut.l) {
      out.push_back(Interval{iv.l, std::min(iv.r, cut.l)});
    }
    if (cut.r < iv.r) {
      out.push_back(Interval{std::max(iv.l, cut.r), iv.r});
    }
  }
  return normalize(out);
}

std::vector<Interval> RowModel::intersectIntervals(const std::vector<Interval> &a,
                                                   const std::vector<Interval> &b) {
  std::vector<Interval> out;
  std::size_t i = 0;
  std::size_t j = 0;
  while (i < a.size() && j < b.size()) {
    Interval iv{std::max(a[i].l, b[j].l), std::min(a[i].r, b[j].r)};
    if (iv.l < iv.r) {
      out.push_back(iv);
    }
    if (a[i].r < b[j].r) {
      ++i;
    } else {
      ++j;
    }
  }
  return out;
}

