#include "density_estimator.h"
#include "gp_parser.h"
#include "legalizer.h"
#include "row_interval_builder.h"
#include "tcl_writer.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace legalizer {

struct RunConfig {
    double alpha = 0.0;
    double threshold = 0.0;
    std::string input_path;
    std::string output_path;
};

double parseDoubleArg(const char* text, const char* name) {
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !std::isfinite(value)) {
        throw PlacementError(std::string("invalid ") + name + ": " + text);
    }
    return value;
}

RunConfig parseArgs(int argc, char** argv) {
    if (argc != 5) {
        throw PlacementError("usage: ./Legalizer <alpha> <threshold> <input.gp> <output.tcl>");
    }
    RunConfig config;
    config.alpha = parseDoubleArg(argv[1], "alpha");
    config.threshold = parseDoubleArg(argv[2], "threshold");
    if (config.alpha < 0.0 || config.alpha > 1.0) {
        throw PlacementError("alpha must be in [0, 1]");
    }
    if (config.threshold < 0.0 || config.threshold > 100.0) {
        throw PlacementError("threshold must be in [0, 100]");
    }
    config.input_path = argv[3];
    config.output_path = argv[4];
    return config;
}

int run(const RunConfig& config) {
    PlacementModel model = parseGpFile(config.input_path);
    std::vector<RowInterval> intervals = buildRowIntervals(model);
    LegalizationResult solved = legalize(model, intervals, config.alpha, config.threshold);
    ValidationResult valid =
        validatePlacement(model, solved.placements, intervals, config.alpha, config.threshold);
    if (!valid.ok) {
        std::cerr << "validation failed:";
        for (const std::string& error : valid.errors) {
            std::cerr << "\n  " << error;
        }
        std::cerr << "\n";
        return 2;
    }
    writeTcl(model, solved.placements, config.output_path);
    std::cout << "Legalizer metrics: avg_disp_um=" << valid.metrics.avg_displacement_um
              << " norm_disp=" << valid.metrics.normalized_displacement
              << " dor=" << valid.metrics.dor_percent
              << " flow_quality=" << valid.metrics.flow_quality << "\n";
    return 0;
}

}  // namespace legalizer

int main(int argc, char** argv) {
    try {
        const legalizer::RunConfig config = legalizer::parseArgs(argc, argv);
        return legalizer::run(config);
    } catch (const std::exception& e) {
        std::cerr << "Legalizer error: " << e.what() << "\n";
        return 1;
    }
}
