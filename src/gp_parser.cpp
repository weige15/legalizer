#include "gp_parser.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace legalizer {
namespace {

bool parseDbu(const std::string &text, Dbu *value) {
  std::size_t pos = 0;
  try {
    long long v = std::stoll(text, &pos, 10);
    if (pos != text.size()) {
      return false;
    }
    *value = v;
    return true;
  } catch (...) {
    return false;
  }
}

bool parseInt(const std::string &text, int *value) {
  Dbu dbu = 0;
  if (!parseDbu(text, &dbu)) {
    return false;
  }
  *value = static_cast<int>(dbu);
  return static_cast<Dbu>(*value) == dbu;
}

std::vector<std::string> split(const std::string &line) {
  std::istringstream in(line);
  std::vector<std::string> tokens;
  std::string token;
  while (in >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

Status duplicateOrMark(std::unordered_map<std::string, int> *seen,
                       const std::string &key, int lineNo) {
  if (seen->count(key) != 0) {
    return Status::Error("line " + std::to_string(lineNo) + ": duplicate header " + key);
  }
  (*seen)[key] = lineNo;
  return Status::Ok();
}

Status parseObject(const std::vector<std::string> &tokens, int lineNo,
                   PlacementModel *model) {
  if (tokens.size() < 6 || tokens.size() > 7) {
    return Status::Error("line " + std::to_string(lineNo) + ": malformed object record");
  }

  const std::string &typeText = tokens.back();
  Dbu llx = 0;
  Dbu lly = 0;
  Dbu width = 0;
  Dbu height = 0;
  if (!parseDbu(tokens[1], &llx) || !parseDbu(tokens[2], &lly) ||
      !parseDbu(tokens[3], &width) || !parseDbu(tokens[4], &height)) {
    return Status::Error("line " + std::to_string(lineNo) + ": invalid integer field");
  }
  if (width <= 0 || height <= 0) {
    return Status::Error("line " + std::to_string(lineNo) + ": object dimensions must be positive");
  }

  if (typeText == "CELL") {
    if (tokens.size() != 7) {
      return Status::Error("line " + std::to_string(lineNo) + ": CELL record missing orientation");
    }
    Cell cell;
    cell.name = tokens[0];
    cell.original = Point{llx, lly};
    cell.placed = cell.original;
    cell.width = width;
    cell.height = height;
    cell.orient = tokens[5];
    cell.originalOrient = cell.orient;
    model->cells.push_back(cell);
    return Status::Ok();
  }

  if (typeText == "MACRO") {
    if (tokens.size() != 7) {
      return Status::Error("line " + std::to_string(lineNo) + ": MACRO record missing orientation");
    }
    Obstacle obstacle;
    obstacle.name = tokens[0];
    obstacle.rect = Rect{llx, lly, llx + width, lly + height};
    obstacle.type = ObjectType::Macro;
    obstacle.orient = tokens[5];
    model->obstacles.push_back(obstacle);
    return Status::Ok();
  }

  if (typeText == "BLOCKAGE") {
    if (tokens.size() != 6) {
      return Status::Error("line " + std::to_string(lineNo) + ": BLOCKAGE record must omit orientation");
    }
    Obstacle obstacle;
    obstacle.name = tokens[0];
    obstacle.rect = Rect{llx, lly, llx + width, lly + height};
    obstacle.type = ObjectType::Blockage;
    model->obstacles.push_back(obstacle);
    return Status::Ok();
  }

  return Status::Error("line " + std::to_string(lineNo) + ": unknown object type '" +
                       typeText + "'");
}

}  // namespace

Status parseGpFile(const std::string &path, PlacementModel *model) {
  std::ifstream in(path);
  if (!in) {
    return Status::Error("failed to open input file '" + path + "'");
  }

  PlacementModel parsed;
  std::unordered_map<std::string, int> seen;
  bool sawObjectHeader = false;
  std::string line;
  int lineNo = 0;
  while (std::getline(in, line)) {
    ++lineNo;
    std::vector<std::string> tokens = split(line);
    if (tokens.empty()) {
      continue;
    }

    const std::string &key = tokens[0];
    if (key == "Name") {
      if (sawObjectHeader) {
        return Status::Error("line " + std::to_string(lineNo) + ": duplicate object header");
      }
      sawObjectHeader = true;
      continue;
    }

    if (!sawObjectHeader) {
      Status mark = duplicateOrMark(&seen, key, lineNo);
      if (!mark.ok) {
        return mark;
      }
      if (key == "DBU_Per_Micron") {
        int value = 0;
        if (tokens.size() != 2 || !parseInt(tokens[1], &value) || value <= 0) {
          return Status::Error("line " + std::to_string(lineNo) + ": invalid DBU_Per_Micron");
        }
        parsed.tech.dbuPerMicron = value;
      } else if (key == "DieArea_LL") {
        if (tokens.size() != 3 || !parseDbu(tokens[1], &parsed.tech.die.lx) ||
            !parseDbu(tokens[2], &parsed.tech.die.ly)) {
          return Status::Error("line " + std::to_string(lineNo) + ": invalid DieArea_LL");
        }
      } else if (key == "DieArea_UR") {
        if (tokens.size() != 3 || !parseDbu(tokens[1], &parsed.tech.die.ux) ||
            !parseDbu(tokens[2], &parsed.tech.die.uy)) {
          return Status::Error("line " + std::to_string(lineNo) + ": invalid DieArea_UR");
        }
      } else if (key == "Site_Width") {
        if (tokens.size() != 2 || !parseDbu(tokens[1], &parsed.tech.siteWidth) ||
            parsed.tech.siteWidth <= 0) {
          return Status::Error("line " + std::to_string(lineNo) + ": invalid Site_Width");
        }
      } else if (key == "Site_Height") {
        if (tokens.size() != 2 || !parseDbu(tokens[1], &parsed.tech.siteHeight) ||
            parsed.tech.siteHeight <= 0) {
          return Status::Error("line " + std::to_string(lineNo) + ": invalid Site_Height");
        }
      } else {
        return Status::Error("line " + std::to_string(lineNo) + ": unknown header '" + key + "'");
      }
      continue;
    }

    Status status = parseObject(tokens, lineNo, &parsed);
    if (!status.ok) {
      return status;
    }
  }

  const char *required[] = {"DBU_Per_Micron", "DieArea_LL", "DieArea_UR",
                            "Site_Width", "Site_Height"};
  for (const char *key : required) {
    if (seen.count(key) == 0) {
      return Status::Error(std::string("missing required header ") + key);
    }
  }
  if (!sawObjectHeader) {
    return Status::Error("missing object header");
  }
  Status techStatus = validateTech(parsed.tech);
  if (!techStatus.ok) {
    return techStatus;
  }

  *model = parsed;
  return Status::Ok();
}

}  // namespace legalizer
