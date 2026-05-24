#include "row_interval_builder.h"

#include <algorithm>

namespace legalizer {
namespace {

struct Span {
  Dbu lx = 0;
  Dbu ux = 0;
};

}  // namespace

Status buildRowIntervals(const PlacementModel &model, std::vector<Row> *rows) {
  rows->clear();
  Status techStatus = validateTech(model.tech);
  if (!techStatus.ok) {
    return techStatus;
  }

  std::vector<Obstacle> obstacles = model.obstacles;
  std::sort(obstacles.begin(), obstacles.end(),
            [](const Obstacle &a, const Obstacle &b) {
              if (a.rect.ly != b.rect.ly) return a.rect.ly < b.rect.ly;
              if (a.rect.lx != b.rect.lx) return a.rect.lx < b.rect.lx;
              return a.name < b.name;
            });

  const int rowsCount = rowCount(model.tech);
  rows->reserve(rowsCount);
  for (int rowIdx = 0; rowIdx < rowsCount; ++rowIdx) {
    Row row;
    row.rowIndex = rowIdx;
    row.y = rowY(model.tech, rowIdx);
    Rect rowBand{model.tech.die.lx, row.y, model.tech.die.ux,
                 row.y + model.tech.siteHeight};

    std::vector<Span> blocked;
    for (const Obstacle &obs : obstacles) {
      if (!overlaps(rowBand, obs.rect)) {
        continue;
      }
      Dbu lx = std::max(model.tech.die.lx, obs.rect.lx);
      Dbu ux = std::min(model.tech.die.ux, obs.rect.ux);
      if (ux <= lx) {
        continue;
      }
      lx = std::max(model.tech.die.lx, snapDownToSite(model.tech, lx));
      ux = std::min(model.tech.die.ux, snapUpToSite(model.tech, ux));
      if (ux > lx) {
        blocked.push_back(Span{lx, ux});
      }
    }
    std::sort(blocked.begin(), blocked.end(), [](const Span &a, const Span &b) {
      if (a.lx != b.lx) return a.lx < b.lx;
      return a.ux < b.ux;
    });

    std::vector<Span> merged;
    for (const Span &span : blocked) {
      if (merged.empty() || span.lx > merged.back().ux) {
        merged.push_back(span);
      } else {
        merged.back().ux = std::max(merged.back().ux, span.ux);
      }
    }

    Dbu cursor = snapUpToSite(model.tech, model.tech.die.lx);
    int intervalIdx = 0;
    for (const Span &span : merged) {
      Dbu xMin = snapUpToSite(model.tech, cursor);
      Dbu xMax = snapDownToSite(model.tech, span.lx);
      if (xMax > xMin && xMax - xMin >= model.tech.siteWidth) {
        row.intervals.push_back(RowInterval{rowIdx, intervalIdx++, row.y, xMin, xMax, 0, {}});
      }
      cursor = std::max(cursor, span.ux);
    }
    Dbu xMin = snapUpToSite(model.tech, cursor);
    Dbu xMax = snapDownToSite(model.tech, model.tech.die.ux);
    if (xMax > xMin && xMax - xMin >= model.tech.siteWidth) {
      row.intervals.push_back(RowInterval{rowIdx, intervalIdx++, row.y, xMin, xMax, 0, {}});
    }

    rows->push_back(row);
  }
  return Status::Ok();
}

void recomputeOccupiedWidth(const PlacementModel &model, RowInterval *interval) {
  Dbu width = 0;
  for (int id : interval->cellIds) {
    width += model.cells.at(static_cast<std::size_t>(id)).width;
  }
  interval->occupiedWidth = width;
}

RowInterval *findContainingInterval(std::vector<Row> *rows, int cellId,
                                    const PlacementModel &model) {
  const Cell &cell = model.cells.at(static_cast<std::size_t>(cellId));
  int rowIdx = rowIndexForY(model.tech, cell.placed.y);
  if (rowIdx < 0 || rowIdx >= static_cast<int>(rows->size())) {
    return nullptr;
  }
  for (RowInterval &interval : rows->at(static_cast<std::size_t>(rowIdx)).intervals) {
    if (cell.placed.x >= interval.xMin &&
        cell.placed.x + cell.width <= interval.xMax) {
      return &interval;
    }
  }
  return nullptr;
}

const RowInterval *findContainingInterval(const std::vector<Row> &rows,
                                          const Cell &cell,
                                          const Tech &tech) {
  int rowIdx = rowIndexForY(tech, cell.placed.y);
  if (rowIdx < 0 || rowIdx >= static_cast<int>(rows.size())) {
    return nullptr;
  }
  for (const RowInterval &interval : rows.at(static_cast<std::size_t>(rowIdx)).intervals) {
    if (cell.placed.x >= interval.xMin &&
        cell.placed.x + cell.width <= interval.xMax) {
      return &interval;
    }
  }
  return nullptr;
}

}  // namespace legalizer
