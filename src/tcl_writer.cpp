#include "tcl_writer.h"

#include <fstream>
#include <iomanip>
#include <stdexcept>

void writeTcl(const Design &design, const std::string &path) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("cannot open output file: " + path);
  }
  out << std::fixed << std::setprecision(6);
  for (const Cell &cell : design.cells) {
    if (!cell.placed) {
      throw std::runtime_error("refusing to write unplaced cell: " + cell.name);
    }
    out << "place_cell -inst_name " << cell.name << " -orient R0 -origin {"
        << dbuToMicron(cell.x, design.dbuPerMicron) << " "
        << dbuToMicron(cell.y, design.dbuPerMicron) << "}\n";
  }
}

