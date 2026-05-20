#ifndef LEGALIZER_H
#define LEGALIZER_H

#include "density_estimator.h"
#include "placement_model.h"
#include "row_interval_builder.h"

#include <string>
#include <vector>

namespace legalizer {

struct LegalizeOptions {
  double alpha = 0.7;
  double threshold = 45.0;
  double norm_factor = 18.2;
};

struct LegalizeResult {
  bool ok = false;
  PlacementModel model;
  double average_displacement = 0.0;
  double dor = 0.0;
  double quality = 0.0;
  std::string error;
};

struct ValidationResult {
  bool ok = false;
  double average_displacement = 0.0;
  double dor = 0.0;
  double quality = 0.0;
  std::string error;
};

std::vector<int> cellOrder(const PlacementModel &model, bool reverse);
LegalizeResult legalizePlacement(const PlacementModel &model,
                                 const std::vector<SiteRow> &rows,
                                 const LegalizeOptions &options);
ValidationResult validatePlacement(const PlacementModel &model,
                                   const std::vector<SiteRow> &rows,
                                   const LegalizeOptions &options);
double averageDisplacementMicron(const PlacementModel &model);

}  // namespace legalizer

#endif
