#ifndef LEGALIZER_H
#define LEGALIZER_H

#include <map>
#include <string>
#include <vector>

#include "density_estimator.h"
#include "placement_model.h"
#include "row_interval_builder.h"

namespace legalizer {

struct IntervalSolveResult {
  bool ok = false;
  std::string message;
  std::vector<Dbu> xByOrder;
  long double cost = 0.0;
};

IntervalSolveResult solveIntervalAbacus(const PlacementModel &model,
                                         const RowInterval &interval,
                                         const std::vector<int> &orderedCellIds);

Status legalizePlacement(PlacementModel *model, std::vector<Row> *rows);
Status tetrisPlaceCell(PlacementModel *model, std::vector<Row> *rows, int cellId);
Status runDorRepair(PlacementModel *model, std::vector<Row> *rows, double alpha,
                    double threshold);
std::vector<std::string> validateLegality(const PlacementModel &model,
                                          const std::vector<Row> &rows);

}  // namespace legalizer

#endif
