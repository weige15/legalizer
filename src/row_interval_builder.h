#ifndef ROW_INTERVAL_BUILDER_H
#define ROW_INTERVAL_BUILDER_H

#include <vector>

#include "placement_model.h"

namespace legalizer {

struct RowInterval {
  int rowIndex = 0;
  int intervalIndex = 0;
  Dbu y = 0;
  Dbu xMin = 0;
  Dbu xMax = 0;
  Dbu occupiedWidth = 0;
  std::vector<int> cellIds;
};

struct Row {
  int rowIndex = 0;
  Dbu y = 0;
  std::vector<RowInterval> intervals;
};

Status buildRowIntervals(const PlacementModel &model, std::vector<Row> *rows);
RowInterval *findContainingInterval(std::vector<Row> *rows, int cellId,
                                    const PlacementModel &model);
const RowInterval *findContainingInterval(const std::vector<Row> &rows,
                                          const Cell &cell,
                                          const Tech &tech);
void recomputeOccupiedWidth(const PlacementModel &model, RowInterval *interval);

}  // namespace legalizer

#endif
