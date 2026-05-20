#ifndef TCL_WRITER_H
#define TCL_WRITER_H

#include "placement_model.h"

#include <string>
#include <vector>

namespace legalizer {

void writeTcl(const PlacementModel& model, const std::vector<Point>& placements,
              const std::string& output_path);

}  // namespace legalizer

#endif
