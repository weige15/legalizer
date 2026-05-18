#include "gp_parser.h"

#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
Coord parseCoordToken(const std::string &token, int lineNo, const char *field) {
  std::size_t pos = 0;
  long long value = 0;
  try {
    value = std::stoll(token, &pos, 10);
  } catch (const std::exception &) {
    throw std::runtime_error("line " + std::to_string(lineNo) + ": invalid integer in " + field);
  }
  if (pos != token.size()) {
    throw std::runtime_error("line " + std::to_string(lineNo) + ": trailing characters in " + field);
  }
  return value;
}

int parsePositiveIntToken(const std::string &token, int lineNo, const char *field) {
  const Coord value = parseCoordToken(token, lineNo, field);
  if (value <= 0 || value > std::numeric_limits<int>::max()) {
    throw std::runtime_error("line " + std::to_string(lineNo) + ": invalid positive integer in " + field);
  }
  return static_cast<int>(value);
}

Coord checkedAdd(Coord a, Coord b, int lineNo, const char *field) {
  if ((b > 0 && a > std::numeric_limits<Coord>::max() - b) ||
      (b < 0 && a < std::numeric_limits<Coord>::min() - b)) {
    throw std::runtime_error("line " + std::to_string(lineNo) + ": coordinate overflow in " + field);
  }
  return a + b;
}

ObjectType parseType(const std::string &token, int lineNo) {
  if (token == "CELL") {
    return ObjectType::Cell;
  }
  if (token == "MACRO") {
    return ObjectType::Macro;
  }
  if (token == "BLOCKAGE") {
    return ObjectType::Blockage;
  }
  throw std::runtime_error("line " + std::to_string(lineNo) + ": unknown object type " + token);
}
} // namespace

Design parseGpFile(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot open input file: " + path);
  }

  Design d;
  bool haveDbu = false;
  bool haveLL = false;
  bool haveUR = false;
  bool haveSW = false;
  bool haveSH = false;
  bool inRecords = false;
  std::string line;
  int lineNo = 0;
  std::size_t inputIndex = 0;

  while (std::getline(in, line)) {
    ++lineNo;
    std::istringstream iss(line);
    std::string first;
    if (!(iss >> first)) {
      continue;
    }

    if (!inRecords) {
      if (first == "Name") {
        std::string llx, lly, width, height, type;
        if (!(iss >> llx >> lly >> width >> height >> type) || llx != "LLX" || lly != "LLY" ||
            width != "Width" || height != "Height" || type != "Type") {
          throw std::runtime_error("line " + std::to_string(lineNo) + ": malformed record header");
        }
        std::string extra;
        if (iss >> extra) {
          throw std::runtime_error("line " + std::to_string(lineNo) + ": extra header field");
        }
        inRecords = true;
        continue;
      }

      if (first == "DBU_Per_Micron") {
        std::string v, extra;
        if (!(iss >> v) || (iss >> extra)) {
          throw std::runtime_error("line " + std::to_string(lineNo) + ": malformed DBU_Per_Micron");
        }
        d.dbuPerMicron = parsePositiveIntToken(v, lineNo, "DBU_Per_Micron");
        haveDbu = true;
      } else if (first == "DieArea_LL") {
        std::string x, y, extra;
        if (!(iss >> x >> y) || (iss >> extra)) {
          throw std::runtime_error("line " + std::to_string(lineNo) + ": malformed DieArea_LL");
        }
        d.die.llx = parseCoordToken(x, lineNo, "DieArea_LL.x");
        d.die.lly = parseCoordToken(y, lineNo, "DieArea_LL.y");
        haveLL = true;
      } else if (first == "DieArea_UR") {
        std::string x, y, extra;
        if (!(iss >> x >> y) || (iss >> extra)) {
          throw std::runtime_error("line " + std::to_string(lineNo) + ": malformed DieArea_UR");
        }
        d.die.urx = parseCoordToken(x, lineNo, "DieArea_UR.x");
        d.die.ury = parseCoordToken(y, lineNo, "DieArea_UR.y");
        haveUR = true;
      } else if (first == "Site_Width") {
        std::string v, extra;
        if (!(iss >> v) || (iss >> extra)) {
          throw std::runtime_error("line " + std::to_string(lineNo) + ": malformed Site_Width");
        }
        d.siteWidth = parsePositiveIntToken(v, lineNo, "Site_Width");
        haveSW = true;
      } else if (first == "Site_Height") {
        std::string v, extra;
        if (!(iss >> v) || (iss >> extra)) {
          throw std::runtime_error("line " + std::to_string(lineNo) + ": malformed Site_Height");
        }
        d.siteHeight = parsePositiveIntToken(v, lineNo, "Site_Height");
        haveSH = true;
      } else {
        throw std::runtime_error("line " + std::to_string(lineNo) + ": unexpected metadata key " + first);
      }
      continue;
    }

    std::string name = first;
    std::string sx, sy, sw, sh, stype, extra;
    if (!(iss >> sx >> sy >> sw >> sh >> stype) || (iss >> extra)) {
      throw std::runtime_error("line " + std::to_string(lineNo) + ": malformed object record");
    }
    const Coord x = parseCoordToken(sx, lineNo, "LLX");
    const Coord y = parseCoordToken(sy, lineNo, "LLY");
    const Coord w = parseCoordToken(sw, lineNo, "Width");
    const Coord h = parseCoordToken(sh, lineNo, "Height");
    if (w <= 0 || h <= 0) {
      throw std::runtime_error("line " + std::to_string(lineNo) + ": non-positive object dimensions");
    }
    const ObjectType type = parseType(stype, lineNo);
    Rect rect{x, y, checkedAdd(x, w, lineNo, "URX"), checkedAdd(y, h, lineNo, "URY")};
    if (type == ObjectType::Cell) {
      d.cells.push_back(Cell{name, rect, inputIndex, false, 0, 0});
    } else {
      d.obstacles.push_back(Obstacle{name, rect, type, inputIndex});
    }
    ++inputIndex;
  }

  if (!haveDbu || !haveLL || !haveUR || !haveSW || !haveSH || !inRecords) {
    throw std::runtime_error("input is missing required metadata or record header");
  }
  if (d.dbuPerMicron <= 0 || d.siteWidth <= 0 || d.siteHeight <= 0 || !validRect(d.die)) {
    throw std::runtime_error("input has invalid DBU, die, or site dimensions");
  }
  for (const Cell &c : d.cells) {
    if (c.original.width() > d.die.width() || c.original.height() > d.die.height()) {
      throw std::runtime_error("cell " + c.name + " is larger than the die");
    }
  }
  return d;
}
