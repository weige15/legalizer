#ifndef TCL_WRITER_H
#define TCL_WRITER_H

#include "placement_model.h"

#include <string>

namespace legalizer {

void writeTclFile(const Design& design, const std::string& path);

} // namespace legalizer

#endif
