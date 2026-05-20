#ifndef GP_PARSER_H
#define GP_PARSER_H

#include "placement_model.h"

#include <string>

namespace legalizer {

PlacementModel parseGpFile(const std::string& path);

}  // namespace legalizer

#endif
