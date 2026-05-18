#pragma once

#include "design.h"

#include <string>
#include <vector>

class RowModel {
public:
  explicit RowModel(const Design &design);

  int rowCount() const { return static_cast<int>(rows_.size()); }
  Coord rowY(int row) const { return rows_.at(row).y; }
  int rowIndexForY(Coord y) const;
  int nearestRow(Coord y) const;
  int rowsNeeded(const Cell &cell) const;

  bool baseCanPlace(const Cell &cell, Coord x, Coord y) const;
  bool canPlace(const Cell &cell, Coord x, Coord y) const;
  bool commit(const Cell &cell, Coord x, Coord y);
  bool uncommit(const Cell &cell, Coord x, Coord y);
  std::vector<Interval> availableSegments(const Cell &cell, int row) const;
  std::vector<Coord> candidateXs(const Cell &cell, int row, Coord targetX, int perSegmentRadius) const;

private:
  struct Row {
    Coord y = 0;
    std::vector<Interval> free;
    std::vector<Interval> occupied;
  };

  const Design &design_;
  std::vector<Row> rows_;

  bool legalSite(Coord x, Coord y) const;
  bool intervalContained(const std::vector<Interval> &intervals, Interval q) const;
  static std::vector<Interval> normalize(std::vector<Interval> intervals);
  static std::vector<Interval> subtractInterval(const std::vector<Interval> &intervals, Interval cut);
  static std::vector<Interval> intersectIntervals(const std::vector<Interval> &a,
                                                  const std::vector<Interval> &b);
};

