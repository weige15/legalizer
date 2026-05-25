#ifndef TCL_WRITER_H
#define TCL_WRITER_H

#include <string>

#include "placement_model.h"

namespace legalizer {

Status writeTcl(const PlacementModel &model, const std::string &outputPath);

}  // namespace legalizer

#endif
