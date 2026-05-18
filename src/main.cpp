#include <cstdlib>
#include <iostream>
#include <string>

#include "density_estimator.hpp"
#include "gp_parser.hpp"
#include "legalizer.hpp"
#include "row_interval_builder.hpp"
#include "tcl_writer.hpp"

namespace {

bool parseDoubleFull(const char* text, double& value) {
  if (text == nullptr || *text == '\0') return false;
  char* end = nullptr;
  value = std::strtod(text, &end);
  return end != text && *end == '\0';
}

}  // namespace

int main(int argc, char** argv) {
  using namespace legalizer;

  if (argc != 5) {
    std::cerr << "usage: ./Legalizer <alpha> <threshold> <input.gp> <output.tcl>\n";
    return 1;
  }

  double alpha = 0.0;
  double threshold = 0.0;
  if (!parseDoubleFull(argv[1], alpha) || alpha < 0.0 || alpha > 1.0) {
    std::cerr << "invalid alpha: " << argv[1] << "\n";
    return 1;
  }
  if (!parseDoubleFull(argv[2], threshold) || threshold < 0.0) {
    std::cerr << "invalid threshold: " << argv[2] << "\n";
    return 1;
  }

  std::string error;
  Design design;
  if (!GpParser::parseFile(argv[3], design, error)) {
    std::cerr << "parse error: " << error << "\n";
    return 2;
  }

  std::vector<LegalRow> rows;
  if (!RowIntervalBuilder::build(design, rows, error)) {
    std::cerr << "row build error: " << error << "\n";
    return 3;
  }

  DensityEstimator density(design, threshold);
  Legalizer legalizer(design, rows, density, alpha);
  if (!legalizer.legalize(error)) {
    std::cerr << "legalization error: " << error << "\n";
    return 4;
  }

  if (!TclWriter::writeFile(argv[4], design, error)) {
    std::cerr << "output error: " << error << "\n";
    return 5;
  }

  return 0;
}
