#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "density_estimator.h"
#include "gp_parser.h"
#include "legalizer.h"
#include "row_interval_builder.h"
#include "tcl_writer.h"

namespace {

bool parseDouble(const char *text, double *value) {
  char *end = nullptr;
  *value = std::strtod(text, &end);
  return end != text && *end == '\0' && std::isfinite(*value);
}

void printDiagnostics(const std::vector<std::string> &diagnostics) {
  const std::size_t limit = 12;
  for (std::size_t i = 0; i < diagnostics.size() && i < limit; ++i) {
    std::cerr << "error: " << diagnostics[i] << "\n";
  }
  if (diagnostics.size() > limit) {
    std::cerr << "error: " << (diagnostics.size() - limit)
              << " additional legality diagnostics omitted\n";
  }
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 5) {
    std::cerr << "usage: ./Legalizer <alpha> <threshold> <input.gp> <output.tcl>\n";
    return 2;
  }

  double alpha = 0.0;
  double threshold = 0.0;
  if (!parseDouble(argv[1], &alpha) || alpha < 0.0 || alpha > 1.0) {
    std::cerr << "error: alpha must be finite and in [0, 1]\n";
    return 2;
  }
  if (!parseDouble(argv[2], &threshold)) {
    std::cerr << "error: threshold must be finite\n";
    return 2;
  }

  legalizer::PlacementModel model;
  legalizer::Status status = legalizer::parseGpFile(argv[3], &model);
  if (!status.ok) {
    std::cerr << "error: " << status.message << "\n";
    return 1;
  }
  status = legalizer::validateSupportedCells(model);
  if (!status.ok) {
    std::cerr << "error: " << status.message << "\n";
    return 1;
  }

  std::vector<legalizer::Row> rows;
  status = legalizer::buildRowIntervals(model, &rows);
  if (!status.ok) {
    std::cerr << "error: " << status.message << "\n";
    return 1;
  }
  status = legalizer::legalizePlacement(&model, &rows);
  if (!status.ok) {
    std::cerr << "error: " << status.message << "\n";
    return 1;
  }

  legalizer::Metrics beforeRepair = legalizer::evaluateMetrics(model, alpha, threshold);
  status = legalizer::runDorRepair(&model, &rows, alpha, threshold);
  if (!status.ok) {
    std::cerr << "error: " << status.message << "\n";
    return 1;
  }
  legalizer::Metrics finalMetrics = legalizer::evaluateMetrics(model, alpha, threshold);
  (void)beforeRepair;

  std::vector<std::string> diagnostics = legalizer::validateLegality(model, rows);
  if (!diagnostics.empty()) {
    printDiagnostics(diagnostics);
    return 1;
  }

  status = legalizer::writeTcl(model, argv[4]);
  if (!status.ok) {
    std::cerr << "error: " << status.message << "\n";
    return 1;
  }

  std::cerr << "Legalized " << model.cells.size() << " cells. AvgDisp="
            << finalMetrics.averageDisplacementMicron << " DOR="
            << finalMetrics.dorPercent << " Quality=" << finalMetrics.quality
            << "\n";
  return 0;
}
