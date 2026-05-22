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
  legalizer::PlacementModel baseModel = model;
  std::vector<legalizer::Row> baseRows = rows;

  std::cerr << "Running baseline legalizer (forward order)\n";
  status = legalizer::legalizePlacement(&model, &rows);
  bool forwardOk = status.ok;
  std::string forwardError = status.message;
  legalizer::Metrics forwardMetrics;
  if (forwardOk) {
    forwardMetrics = legalizer::evaluateMetrics(model, alpha, threshold);
    std::cerr << "Forward baseline Quality=" << forwardMetrics.quality
              << " AvgDisp=" << forwardMetrics.averageDisplacementMicron
              << " DOR=" << forwardMetrics.dorPercent << "\n";
  }

  legalizer::PlacementModel reverseModel = baseModel;
  std::vector<legalizer::Row> reverseRows = baseRows;
  std::cerr << "Running baseline legalizer (reverse order)\n";
  status = legalizer::legalizePlacementReverse(&reverseModel, &reverseRows);
  bool reverseOk = status.ok;
  std::string reverseError = status.message;
  legalizer::Metrics reverseMetrics;
  if (reverseOk) {
    reverseMetrics = legalizer::evaluateMetrics(reverseModel, alpha, threshold);
    std::cerr << "Reverse baseline Quality=" << reverseMetrics.quality
              << " AvgDisp=" << reverseMetrics.averageDisplacementMicron
              << " DOR=" << reverseMetrics.dorPercent << "\n";
  }

  if (!forwardOk && !reverseOk) {
    std::cerr << "error: forward legalization failed: " << forwardError << "\n";
    std::cerr << "error: reverse legalization failed: " << reverseError << "\n";
    return 1;
  }
  if (!forwardOk || (reverseOk && reverseMetrics.quality < forwardMetrics.quality)) {
    model = reverseModel;
    rows = reverseRows;
    std::cerr << "Selected reverse baseline\n";
  } else {
    std::cerr << "Selected forward baseline\n";
  }

  legalizer::Metrics beforeRepair = legalizer::evaluateMetrics(model, alpha, threshold);
  std::cerr << "Running DOR repair\n";
  status = legalizer::runDorRepair(&model, &rows, alpha, threshold);
  if (!status.ok) {
    std::cerr << "error: " << status.message << "\n";
    return 1;
  }
  std::cerr << "Evaluating final metrics\n";
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
