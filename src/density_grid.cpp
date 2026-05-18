#include "density_grid.h"

#include <algorithm>
#include <functional>

DensityGrid::DensityGrid(const Design &design, double threshold) : design_(design), threshold_(threshold) {
  binSize_ = std::max<Coord>(1, static_cast<Coord>(10) * design_.dbuPerMicron);
  nx_ = std::max<int>(1, static_cast<int>(ceilDiv(design_.die.width(), binSize_)));
  ny_ = std::max<int>(1, static_cast<int>(ceilDiv(design_.die.height(), binSize_)));
  used_.assign(static_cast<std::size_t>(nx_ * ny_), 0.0);
  excluded_.assign(static_cast<std::size_t>(nx_ * ny_), false);
  for (const Obstacle &obs : design_.obstacles) {
    if (obs.type != ObjectType::Macro) {
      continue;
    }
    forBins(obs.rect, [&](int ix, int iy, double) { excluded_[index(ix, iy)] = true; });
  }
}

Rect DensityGrid::binRect(int ix, int iy) const {
  const Coord x0 = design_.die.llx + static_cast<Coord>(ix) * binSize_;
  const Coord y0 = design_.die.lly + static_cast<Coord>(iy) * binSize_;
  return Rect{x0, y0, std::min(design_.die.urx, x0 + binSize_),
              std::min(design_.die.ury, y0 + binSize_)};
}

void DensityGrid::forBins(const Rect &rect, const std::function<void(int, int, double)> &fn) const {
  const Rect clipped{std::max(rect.llx, design_.die.llx), std::max(rect.lly, design_.die.lly),
                     std::min(rect.urx, design_.die.urx), std::min(rect.ury, design_.die.ury)};
  if (!validRect(clipped)) {
    return;
  }
  const int x0 = std::max(0, static_cast<int>(floorDiv(clipped.llx - design_.die.llx, binSize_)));
  const int y0 = std::max(0, static_cast<int>(floorDiv(clipped.lly - design_.die.lly, binSize_)));
  const int x1 = std::min(nx_ - 1, static_cast<int>(floorDiv(clipped.urx - 1 - design_.die.llx, binSize_)));
  const int y1 = std::min(ny_ - 1, static_cast<int>(floorDiv(clipped.ury - 1 - design_.die.lly, binSize_)));
  for (int iy = y0; iy <= y1; ++iy) {
    for (int ix = x0; ix <= x1; ++ix) {
      const Rect b = binRect(ix, iy);
      const Coord ox = std::max<Coord>(0, std::min(clipped.urx, b.urx) - std::max(clipped.llx, b.llx));
      const Coord oy = std::max<Coord>(0, std::min(clipped.ury, b.ury) - std::max(clipped.lly, b.lly));
      fn(ix, iy, static_cast<double>(ox) * static_cast<double>(oy));
    }
  }
}

double DensityGrid::densityCost(const Cell &cell, Coord x, Coord y) const {
  double cost = 0.0;
  int count = 0;
  forBins(placedRect(cell.original, x, y), [&](int ix, int iy, double area) {
    const int idx = index(ix, iy);
    if (excluded_[idx]) {
      return;
    }
    const Rect b = binRect(ix, iy);
    const double binArea = static_cast<double>(b.width()) * static_cast<double>(b.height());
    const double pct = 100.0 * (used_[idx] + area) / std::max(1.0, binArea);
    cost += std::max(0.0, pct - threshold_);
    ++count;
  });
  return count == 0 ? 0.0 : cost / static_cast<double>(count);
}

void DensityGrid::addCell(const Cell &cell, Coord x, Coord y) {
  forBins(placedRect(cell.original, x, y), [&](int ix, int iy, double area) {
    if (!excluded_[index(ix, iy)]) {
      used_[index(ix, iy)] += area;
    }
  });
}

void DensityGrid::removeCell(const Cell &cell, Coord x, Coord y) {
  forBins(placedRect(cell.original, x, y), [&](int ix, int iy, double area) {
    if (!excluded_[index(ix, iy)]) {
      used_[index(ix, iy)] -= area;
      if (used_[index(ix, iy)] < 1e-6) {
        used_[index(ix, iy)] = 0.0;
      }
    }
  });
}

double DensityGrid::estimateDOR() const {
  int total = 0;
  int overflow = 0;
  for (int iy = 0; iy < ny_; ++iy) {
    for (int ix = 0; ix < nx_; ++ix) {
      const int idx = index(ix, iy);
      if (excluded_[idx]) {
        continue;
      }
      const Rect b = binRect(ix, iy);
      const double binArea = static_cast<double>(b.width()) * static_cast<double>(b.height());
      const double pct = 100.0 * used_[idx] / std::max(1.0, binArea);
      ++total;
      if (pct > threshold_) {
        ++overflow;
      }
    }
  }
  return total == 0 ? 0.0 : 100.0 * static_cast<double>(overflow) / static_cast<double>(total);
}

