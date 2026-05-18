#pragma once

#include "density_grid.h"
#include "row_model.h"

void legalizeDesign(Design &design, RowModel &rows, DensityGrid &density, double alpha);
void refineDesign(Design &design, RowModel &rows, DensityGrid &density, double alpha);

