#include "tcl_writer.h"

#include <cstdio>
#include <fstream>
#include <iomanip>

namespace legalizer {
namespace {

std::string tempPathFor(const std::string &outputPath) {
  std::string dir;
  std::string base = outputPath;
  std::size_t slash = outputPath.find_last_of("/\\");
  if (slash != std::string::npos) {
    dir = outputPath.substr(0, slash + 1);
    base = outputPath.substr(slash + 1);
  }
  return dir + "." + base + ".tmp";
}

}  // namespace

Status writeTcl(const PlacementModel &model, const std::string &outputPath) {
  std::string tmp = tempPathFor(outputPath);
  {
    std::ofstream out(tmp);
    if (!out) {
      return Status::Error("failed to open temporary output '" + tmp + "'");
    }
    out << std::fixed << std::setprecision(6);
    for (const Cell &cell : model.cells) {
      if (!cell.placedValid) {
        continue;
      }
      out << "place_cell -inst_name " << cell.name << " -orient " << cell.orient
          << " -origin {" << dbuToMicron(model.tech, cell.placed.x) << " "
          << dbuToMicron(model.tech, cell.placed.y) << "}\n";
    }
    out.flush();
    if (!out) {
      return Status::Error("failed while writing temporary output '" + tmp + "'");
    }
  }
  if (std::rename(tmp.c_str(), outputPath.c_str()) != 0) {
    return Status::Error("failed to rename temporary output to '" + outputPath + "'");
  }
  return Status::Ok();
}

}  // namespace legalizer
