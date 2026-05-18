#pragma once

#include "row_model.h"

#include <string>

bool validateDesign(const Design &design, const RowModel &baseRows, std::string *error);

