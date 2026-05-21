#ifndef GP_PARSER_H
#define GP_PARSER_H

#include <string>

#include "placement_model.h"

namespace legalizer {

Status parseGpFile(const std::string &path, PlacementModel *model);

}  // namespace legalizer

#endif
