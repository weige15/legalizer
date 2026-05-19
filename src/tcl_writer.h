#ifndef TCL_WRITER_H
#define TCL_WRITER_H

#include "placement_model.h"

#include <string>

namespace legalizer {

struct WriteResult {
  bool ok = false;
  std::string error;
};

std::string formatMicron(double value);
bool isSafeTclBracedName(const std::string &name);
WriteResult writePlacementTcl(const PlacementModel &model, const std::string &path);

}  // namespace legalizer

#endif
