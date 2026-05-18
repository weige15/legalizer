#pragma once

#include "design.h"

#include <functional>
#include <vector>

class DensityGrid {
public:
  DensityGrid(const Design &design, double threshold);

  double densityCost(const Cell &cell, Coord x, Coord y) const;
  void addCell(const Cell &cell, Coord x, Coord y);
  void removeCell(const Cell &cell, Coord x, Coord y);
  double estimateDOR() const;

private:
  const Design &design_;
  double threshold_ = 0.0;
  Coord binSize_ = 1;
  int nx_ = 1;
  int ny_ = 1;
  std::vector<double> used_;
  std::vector<bool> excluded_;

  int index(int ix, int iy) const { return iy * nx_ + ix; }
  Rect binRect(int ix, int iy) const;
  void forBins(const Rect &rect, const std::function<void(int, int, double)> &fn) const;
};
