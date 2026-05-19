#include "gp_parser.h"
#include "legalizer.h"
#include "row_interval_builder.h"
#include "tcl_writer.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

double parseFiniteDouble(const char* text, const std::string& name) {
    char* end = nullptr;
    errno = 0;
    double value = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !std::isfinite(value)) {
        throw std::runtime_error("invalid " + name + ": " + text);
    }
    return value;
}

std::string tempPathFor(const std::string& output_path) {
    return output_path + ".tmp";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: " << argv[0] << " <alpha> <threshold> <input.gp> <output.tcl>\n";
        return 1;
    }

    try {
        legalizer::LegalizerConfig config;
        config.alpha = parseFiniteDouble(argv[1], "alpha");
        config.threshold = parseFiniteDouble(argv[2], "threshold");
        std::string input_path = argv[3];
        std::string output_path = argv[4];

        legalizer::Design design = legalizer::parseGpFile(input_path);
        std::vector<legalizer::Row> rows = legalizer::buildRowSegments(design);
        legalizer::legalizeDesign(design, rows, config);
        std::string legality_error;
        if (!legalizer::checkLegality(design, rows, legality_error)) {
            throw std::runtime_error("legality check failed: " + legality_error);
        }

        std::string tmp_path = tempPathFor(output_path);
        std::remove(tmp_path.c_str());
        legalizer::writeTclFile(design, tmp_path);
        if (std::rename(tmp_path.c_str(), output_path.c_str()) != 0) {
            std::remove(tmp_path.c_str());
            throw std::runtime_error("cannot move temporary output into place: " +
                                     std::string(std::strerror(errno)));
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Legalizer error: " << ex.what() << "\n";
        return 1;
    }
}
