#include "tcl_writer.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>

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

bool isBareTclWordSafe(const std::string &text) {
  if (text.empty() || text[0] == '-') {
    return false;
  }
  for (char ch : text) {
    const bool alpha = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    const bool digit = ch >= '0' && ch <= '9';
    const bool symbol = ch == '_' || ch == '/' || ch == '.' || ch == ':' || ch == '-';
    if (!alpha && !digit && !symbol) {
      return false;
    }
  }
  return true;
}

std::string tclWord(const std::string &text) {
  if (isBareTclWordSafe(text)) {
    return text;
  }

  std::string quoted;
  quoted.reserve(text.size() + 2);
  quoted.push_back('{');
  for (char ch : text) {
    if (ch == '\\' || ch == '{' || ch == '}') {
      quoted.push_back('\\');
    }
    quoted.push_back(ch);
  }
  quoted.push_back('}');
  return quoted;
}

Point placeOriginForCell(const Cell &cell) {
  Point origin = cell.placed;
  if (cell.orient == "MX" || cell.orient == "FS") {
    origin.y += cell.height;
  } else if (cell.orient == "MY" || cell.orient == "FN") {
    origin.x += cell.width;
  } else if (cell.orient == "R180" || cell.orient == "S") {
    origin.x += cell.width;
    origin.y += cell.height;
  } else if (cell.orient == "R90" || cell.orient == "E") {
    origin.x += cell.height;
  } else if (cell.orient == "R270" || cell.orient == "W") {
    origin.y += cell.width;
  } else if (cell.orient == "MXR90" || cell.orient == "FE") {
    origin.x += cell.height;
    origin.y += cell.width;
  }
  return origin;
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
      Point origin = placeOriginForCell(cell);
      out << "place_cell -inst_name " << tclWord(cell.name) << " -orient "
          << tclWord(cell.orient) << " -origin {"
          << dbuToMicron(model.tech, origin.x) << " "
          << dbuToMicron(model.tech, origin.y) << "}\n";
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
