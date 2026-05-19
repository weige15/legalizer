#ifndef LEGALIZER_H
#define LEGALIZER_H

#include "density_estimator.h"
#include "row_interval_builder.h"

#include <map>
#include <string>
#include <vector>

namespace legalizer {

struct LegalizerConfig {
    double alpha = 0.7;
    double threshold = 45.0;
};

struct RowCell {
    std::size_t cell_index = 0;
    Coord original_x = 0;
    Coord width = 0;
    std::size_t input_index = 0;
};

struct RowPlacementResult {
    bool feasible = false;
    std::map<std::size_t, Coord> origins;
};

RowPlacementResult placeRowAbacus(const std::vector<RowCell>& cells,
                                  const Interval& segment,
                                  Coord site_origin,
                                  Coord site_width);

void legalizeDesign(Design& design,
                    const std::vector<Row>& rows,
                    const LegalizerConfig& config);

bool checkLegality(const Design& design,
                   const std::vector<Row>& rows,
                   std::string& error);

} // namespace legalizer

#endif
