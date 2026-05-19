#include "gp_parser.h"
#include "legalizer.h"
#include "row_interval_builder.h"
#include "tcl_writer.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

bool parseDoubleStrict(const char *text, double *value) {
  errno = 0;
  char *end = nullptr;
  double parsed = std::strtod(text, &end);
  if (errno != 0 || end == text || *end != '\0' || !std::isfinite(parsed)) {
    return false;
  }
  *value = parsed;
  return true;
}

void usage(const char *argv0) {
  std::cerr << "Usage: " << argv0 << " <alpha> <threshold> <input.gp> <output.tcl>\n";
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 5) {
    usage(argv[0]);
    return 2;
  }

  legalizer::LegalizeOptions options;
  if (!parseDoubleStrict(argv[1], &options.alpha)) {
    std::cerr << "Error: alpha must be a finite number\n";
    return 2;
  }
  if (options.alpha < 0.0 || options.alpha > 1.0) {
    std::cerr << "Error: alpha must be in [0, 1]\n";
    return 2;
  }
  if (!parseDoubleStrict(argv[2], &options.threshold)) {
    std::cerr << "Error: threshold must be a finite number\n";
    return 2;
  }

  legalizer::ParseResult parse = legalizer::parseGpFile(argv[3]);
  if (!parse.ok) {
    std::cerr << "Parse error: " << parse.error << "\n";
    return 1;
  }

  legalizer::RowBuildResult rows = legalizer::buildRowIntervals(parse.model);
  if (!rows.ok) {
    std::cerr << "Row interval error: " << rows.error << "\n";
    return 1;
  }

  legalizer::LegalizeResult legalized =
      legalizer::legalizePlacement(parse.model, rows.rows, options);
  if (!legalized.ok) {
    std::cerr << "Legalization error: " << legalized.error << "\n";
    return 1;
  }

  legalizer::WriteResult write = legalizer::writePlacementTcl(legalized.model, argv[4]);
  if (!write.ok) {
    std::cerr << "Write error: " << write.error << "\n";
    return 1;
  }

  std::cerr << "Legalized " << legalized.model.cells.size()
            << " cells. Avg displacement: " << legalized.average_displacement
            << "um, DOR: " << legalized.dor << "%, quality: " << legalized.quality
            << "\n";
  return 0;
}
