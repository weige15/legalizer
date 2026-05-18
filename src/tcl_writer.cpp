#include "tcl_writer.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace legalizer {
namespace {

std::string formatMicron(int64_t dbu, int64_t dbu_per_micron) {
  if (dbu % dbu_per_micron == 0) {
    return std::to_string(dbu / dbu_per_micron);
  }
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(6)
      << static_cast<double>(dbu) / static_cast<double>(dbu_per_micron);
  std::string out = oss.str();
  while (out.size() > 1 && out.back() == '0') out.pop_back();
  if (!out.empty() && out.back() == '.') out.pop_back();
  return out;
}

bool simpleTclWord(const std::string& value) {
  if (value.empty()) return false;
  for (char ch : value) {
    const bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                    (ch >= '0' && ch <= '9') || ch == '_' || ch == '/' ||
                    ch == '.' || ch == '-' || ch == '$';
    if (!ok) return false;
  }
  return true;
}

std::string tclWord(const std::string& value) {
  if (simpleTclWord(value)) return value;
  std::string out = "{";
  for (char ch : value) {
    if (ch == '}' || ch == '\\') out.push_back('\\');
    out.push_back(ch);
  }
  out.push_back('}');
  return out;
}

}  // namespace

bool TclWriter::writeFile(const std::string& path, const Design& design,
                          std::string& error) {
  if (design.dbu_per_micron <= 0) {
    error = "invalid DBU scale for TCL output";
    return false;
  }

  std::ofstream out(path);
  if (!out) {
    error = "cannot open output file: " + path;
    return false;
  }

  for (const Cell& cell : design.cells) {
    if (!cell.has_placement) {
      error = "cell has no final placement: " + cell.name;
      return false;
    }
    out << "place_cell -inst_name " << tclWord(cell.name)
        << " -orient R0 -origin {"
        << formatMicron(cell.placed.x_min, design.dbu_per_micron) << " "
        << formatMicron(cell.placed.y_min, design.dbu_per_micron) << "}\n";
  }
  out.flush();
  if (!out) {
    error = "failed while writing output file: " + path;
    return false;
  }
  return true;
}

}  // namespace legalizer
