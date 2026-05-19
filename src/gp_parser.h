#ifndef GP_PARSER_H
#define GP_PARSER_H

#include "placement_model.h"

#include <string>

namespace legalizer {

struct ParseResult {
  bool ok = false;
  PlacementModel model;
  std::string error;
};

ParseResult parseGpFile(const std::string &path);

}  // namespace legalizer

#endif
