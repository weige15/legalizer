#include "legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
struct Candidate {
  bool valid = false;
  Coord x = 0;
  Coord y = 0;
  int row = 0;
  double score = 0.0;
  double disp = 0.0;
  int rowDistance = 0;
};

bool betterCandidate(const Candidate &a, const Candidate &b) {
  constexpr double eps = 1e-9;
  if (!b.valid) {
    return true;
  }
  if (std::fabs(a.score - b.score) > eps) {
    return a.score < b.score;
  }
  if (std::fabs(a.disp - b.disp) > eps) {
    return a.disp < b.disp;
  }
  if (a.rowDistance != b.rowDistance) {
    return a.rowDistance < b.rowDistance;
  }
  if (a.x != b.x) {
    return a.x < b.x;
  }
  return a.row < b.row;
}

std::vector<int> rowOrder(const RowModel &rows, int target, int limit) {
  std::vector<int> order;
  if (target < 0) {
    target = 0;
  }
  const int maxRadius = limit < 0 ? rows.rowCount() : std::min(rows.rowCount(), limit);
  for (int delta = 0; delta < rows.rowCount() && static_cast<int>(order.size()) < maxRadius; ++delta) {
    const int a = target - delta;
    const int b = target + delta;
    if (a >= 0) {
      order.push_back(a);
    }
    if (delta != 0 && b < rows.rowCount()) {
      order.push_back(b);
    }
  }
  return order;
}

Candidate bestForCell(const Cell &cell, const Design &design, RowModel &rows, DensityGrid &density,
                      double alpha, int rowLimit, int xRadius) {
  Candidate best;
  const int targetRow = rows.nearestRow(cell.original.lly);
  for (int row : rowOrder(rows, targetRow, rowLimit)) {
    const Coord y = rows.rowY(row);
    const int rowDistance = std::abs(row - targetRow);
    for (Coord x : rows.candidateXs(cell, row, cell.original.llx, xRadius)) {
      if (!rows.canPlace(cell, x, y)) {
        continue;
      }
      const double disp = dbuToMicron(manhattan(cell.original, x, y), design.dbuPerMicron);
      const double dens = density.densityCost(cell, x, y);
      Candidate cand{true, x, y, row, alpha * disp + (1.0 - alpha) * dens, disp, rowDistance};
      if (betterCandidate(cand, best)) {
        best = cand;
      }
    }
  }
  return best;
}

double placementScore(const Cell &cell, const Design &design, DensityGrid &density, double alpha, Coord x,
                      Coord y) {
  return alpha * dbuToMicron(manhattan(cell.original, x, y), design.dbuPerMicron) +
         (1.0 - alpha) * density.densityCost(cell, x, y);
}
} // namespace

void legalizeDesign(Design &design, RowModel &rows, DensityGrid &density, double alpha) {
  std::vector<std::size_t> order(design.cells.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](std::size_t ia, std::size_t ib) {
    const Cell &a = design.cells[ia];
    const Cell &b = design.cells[ib];
    const int ar = rows.nearestRow(a.original.lly);
    const int br = rows.nearestRow(b.original.lly);
    if (ar != br) {
      return ar < br;
    }
    if (a.original.llx != b.original.llx) {
      return a.original.llx < b.original.llx;
    }
    if (a.original.width() != b.original.width()) {
      return a.original.width() > b.original.width();
    }
    return a.inputIndex < b.inputIndex;
  });

  for (std::size_t pos = 0; pos < order.size(); ++pos) {
    const std::size_t idx = order[pos];
    Cell &cell = design.cells[idx];
    if (design.cells.size() >= 50000 && pos % 10000 == 0) {
      std::cerr << "Legalizer progress: placed " << pos << " / " << design.cells.size() << "\n";
    }

    Candidate best = bestForCell(cell, design, rows, density, alpha, 11, 8);
    if (!best.valid) {
      best = bestForCell(cell, design, rows, density, alpha, 31, 12);
    }
    if (!best.valid) {
      best = bestForCell(cell, design, rows, density, alpha, -1, 16);
    }
    if (!best.valid || !rows.commit(cell, best.x, best.y)) {
      throw std::runtime_error("cannot legalize cell " + cell.name);
    }
    cell.x = best.x;
    cell.y = best.y;
    cell.placed = true;
    density.addCell(cell, cell.x, cell.y);
  }
  if (design.cells.size() >= 50000) {
    std::cerr << "Legalizer progress: placed " << design.cells.size() << " / " << design.cells.size() << "\n";
  }
}

void refineDesign(Design &design, RowModel &rows, DensityGrid &density, double alpha) {
  if (design.cells.size() > 20000) {
    std::vector<std::size_t> order(design.cells.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t ia, std::size_t ib) {
      const Cell &a = design.cells[ia];
      const Cell &b = design.cells[ib];
      const Coord da = manhattan(a.original, a.x, a.y);
      const Coord db = manhattan(b.original, b.x, b.y);
      if (da != db) {
        return da > db;
      }
      return a.inputIndex < b.inputIndex;
    });

    const std::size_t limit = std::min<std::size_t>(order.size(), 20000);
    std::size_t improved = 0;
    for (std::size_t k = 0; k < limit; ++k) {
      Cell &cell = design.cells[order[k]];
      const Coord oldX = cell.x;
      const Coord oldY = cell.y;
      density.removeCell(cell, oldX, oldY);
      if (!rows.uncommit(cell, oldX, oldY)) {
        density.addCell(cell, oldX, oldY);
        continue;
      }

      const double oldScore = placementScore(cell, design, density, alpha, oldX, oldY);
      Candidate best = bestForCell(cell, design, rows, density, alpha, 21, 24);
      if (best.valid && best.score + 1e-9 < oldScore && rows.commit(cell, best.x, best.y)) {
        cell.x = best.x;
        cell.y = best.y;
        density.addCell(cell, cell.x, cell.y);
        ++improved;
      } else {
        rows.commit(cell, oldX, oldY);
        cell.x = oldX;
        cell.y = oldY;
        density.addCell(cell, oldX, oldY);
      }
    }
    std::cerr << "Legalizer refinement: improved " << improved << " / " << limit
              << " high-displacement cells\n";
    return;
  }
  const int passes = 2;
  for (int pass = 0; pass < passes; ++pass) {
    bool improved = false;
    for (Cell &cell : design.cells) {
      if (!cell.placed) {
        continue;
      }
      const Coord oldX = cell.x;
      const Coord oldY = cell.y;
      density.removeCell(cell, oldX, oldY);
      if (!rows.uncommit(cell, oldX, oldY)) {
        density.addCell(cell, oldX, oldY);
        continue;
      }

      const double oldScore = placementScore(cell, design, density, alpha, oldX, oldY);
      Candidate best = bestForCell(cell, design, rows, density, alpha, 12, 12);
      if (best.valid && best.score + 1e-9 < oldScore && rows.commit(cell, best.x, best.y)) {
        cell.x = best.x;
        cell.y = best.y;
        density.addCell(cell, cell.x, cell.y);
        improved = true;
      } else {
        rows.commit(cell, oldX, oldY);
        cell.x = oldX;
        cell.y = oldY;
        density.addCell(cell, oldX, oldY);
      }
    }
    if (!improved) {
      break;
    }
  }
}
