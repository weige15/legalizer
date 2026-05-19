#ifndef ROW_INTERVAL_BUILDER_H
#define ROW_INTERVAL_BUILDER_H

#include "placement_model.h"

#include <vector>

namespace legalizer {

struct Interval {
    Coord lx = 0;
    Coord ux = 0;
};

struct Row {
    Coord y = 0;
    std::size_t index = 0;
    std::vector<Interval> segments;
};

std::vector<Row> buildRowSegments(const Design& design);
bool intervalFits(const Interval& interval, Coord width);
bool originFitsInInterval(const Interval& interval, Coord origin_x, Coord width);

} // namespace legalizer

#endif
