#include "tcl_writer.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace legalizer {
namespace {

std::string formatMicron(Coord dbu, Coord dbu_per_micron) {
    if (dbu_per_micron <= 0) {
        throw std::runtime_error("invalid DBU_Per_Micron for TCL output");
    }
    long double value = static_cast<long double>(dbu) / static_cast<long double>(dbu_per_micron);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << static_cast<double>(value);
    std::string text = oss.str();
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    if (text == "-0") {
        text = "0";
    }
    return text;
}

} // namespace

void writeTclFile(const Design& design, const std::string& path) {
    validateDesignMetadata(design);
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot open output file: " + path);
    }
    for (const MovableCell& cell : design.cells) {
        if (!cell.legal.has_value()) {
            throw std::runtime_error("cannot write TCL: cell '" + cell.name + "' has no legal placement");
        }
        out << "place_cell -inst_name " << cell.name
            << " -orient R0 -origin {"
            << formatMicron(cell.legal->lx, design.dbu_per_micron)
            << " "
            << formatMicron(cell.legal->ly, design.dbu_per_micron)
            << "}\n";
    }
    if (!out) {
        throw std::runtime_error("failed while writing output file: " + path);
    }
}

} // namespace legalizer
