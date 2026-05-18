#include "config.h"
#include "density_grid.h"
#include "gp_parser.h"
#include "legalizer.h"
#include "row_model.h"
#include "tcl_writer.h"
#include "validation.h"

#include <exception>
#include <iostream>

int main(int argc, char **argv) {
  try {
    Config cfg = parseConfig(argc, argv);
    Design design = parseGpFile(cfg.inputPath);
    RowModel rows(design);
    DensityGrid density(design, cfg.threshold);
    legalizeDesign(design, rows, density, cfg.alpha);
    refineDesign(design, rows, density, cfg.alpha);
    std::string error;
    if (!validateDesign(design, rows, &error)) {
      throw std::runtime_error("internal validation failed: " + error);
    }
    writeTcl(design, cfg.outputPath);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Legalizer error: " << e.what() << "\n";
    return 1;
  }
}

