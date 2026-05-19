#include "gp_parser.h"

#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <vector>

namespace legalizer {
namespace {

std::string trim(const std::string &s) {
  std::size_t first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  std::size_t last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

bool parseInteger(const std::string &token, Coord *value) {
  if (token.empty()) {
    return false;
  }
  std::size_t pos = 0;
  try {
    long long parsed = std::stoll(token, &pos, 10);
    if (pos != token.size()) {
      return false;
    }
    *value = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

std::vector<std::string> split(const std::string &line) {
  std::istringstream iss(line);
  std::vector<std::string> out;
  std::string tok;
  while (iss >> tok) {
    out.push_back(tok);
  }
  return out;
}

bool readRequiredLine(std::ifstream &in, std::string *line, int *line_no) {
  while (std::getline(in, *line)) {
    ++(*line_no);
    if (!trim(*line).empty()) {
      return true;
    }
  }
  return false;
}

bool parseOneValue(const std::vector<std::string> &tokens, const std::string &key,
                   Coord *value, std::string *error, int line_no) {
  if (tokens.size() != 2 || tokens[0] != key || !parseInteger(tokens[1], value)) {
    *error = "line " + std::to_string(line_no) + ": expected '" + key + " <integer>'";
    return false;
  }
  return true;
}

bool parseTwoValues(const std::vector<std::string> &tokens, const std::string &key,
                    Coord *a, Coord *b, std::string *error, int line_no) {
  if (tokens.size() != 3 || tokens[0] != key || !parseInteger(tokens[1], a) ||
      !parseInteger(tokens[2], b)) {
    *error = "line " + std::to_string(line_no) + ": expected '" + key +
             " <integer> <integer>'";
    return false;
  }
  return true;
}

ParseResult fail(const std::string &msg) {
  ParseResult result;
  result.error = msg;
  return result;
}

}  // namespace

ParseResult parseGpFile(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    return fail("cannot open input file '" + path + "'");
  }

  PlacementModel model;
  std::string line;
  int line_no = 0;
  std::string error;
  Coord llx = 0;
  Coord lly = 0;
  Coord urx = 0;
  Coord ury = 0;
  Coord value = 0;

  const std::vector<std::string> keys = {
      "DBU_Per_Micron", "DieArea_LL", "DieArea_UR", "Site_Width", "Site_Height"};
  for (const std::string &key : keys) {
    if (!readRequiredLine(in, &line, &line_no)) {
      return fail("missing metadata line for " + key);
    }
    std::vector<std::string> tokens = split(line);
    if (key == "DBU_Per_Micron") {
      if (!parseOneValue(tokens, key, &value, &error, line_no)) {
        return fail(error);
      }
      if (value <= 0 || value > std::numeric_limits<int>::max()) {
        return fail("line " + std::to_string(line_no) + ": DBU_Per_Micron must be positive");
      }
      model.dbu_per_micron = static_cast<int>(value);
    } else if (key == "DieArea_LL") {
      if (!parseTwoValues(tokens, key, &llx, &lly, &error, line_no)) {
        return fail(error);
      }
    } else if (key == "DieArea_UR") {
      if (!parseTwoValues(tokens, key, &urx, &ury, &error, line_no)) {
        return fail(error);
      }
      model.die = Rect{llx, lly, urx, ury};
      if (!isValid(model.die)) {
        return fail("line " + std::to_string(line_no) + ": die area must have positive size");
      }
    } else if (key == "Site_Width") {
      if (!parseOneValue(tokens, key, &value, &error, line_no)) {
        return fail(error);
      }
      if (value <= 0) {
        return fail("line " + std::to_string(line_no) + ": Site_Width must be positive");
      }
      model.site_width = value;
    } else if (key == "Site_Height") {
      if (!parseOneValue(tokens, key, &value, &error, line_no)) {
        return fail(error);
      }
      if (value <= 0) {
        return fail("line " + std::to_string(line_no) + ": Site_Height must be positive");
      }
      model.site_height = value;
    }
  }

  if (!readRequiredLine(in, &line, &line_no)) {
    return fail("missing instance table header");
  }
  if (split(line) != std::vector<std::string>{"Name", "LLX", "LLY", "Width", "Height", "Type"}) {
    return fail("line " + std::to_string(line_no) +
                ": expected header 'Name LLX LLY Width Height Type'");
  }

  std::set<std::string> names;
  std::size_t input_index = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (trim(line).empty()) {
      continue;
    }
    std::vector<std::string> tokens = split(line);
    if (tokens.size() != 6) {
      return fail("line " + std::to_string(line_no) + ": expected six instance fields");
    }
    if (!names.insert(tokens[0]).second) {
      return fail("line " + std::to_string(line_no) + ": duplicate instance name '" + tokens[0] + "'");
    }
    Coord x = 0;
    Coord y = 0;
    Coord w = 0;
    Coord h = 0;
    if (!parseInteger(tokens[1], &x) || !parseInteger(tokens[2], &y) ||
        !parseInteger(tokens[3], &w) || !parseInteger(tokens[4], &h)) {
      return fail("line " + std::to_string(line_no) + ": malformed integer in instance row");
    }
    if (w <= 0 || h <= 0) {
      return fail("line " + std::to_string(line_no) + ": instance dimensions must be positive");
    }
    Rect rect = makeRect(x, y, w, h);
    if (tokens[5] == "CELL") {
      Cell cell;
      cell.name = tokens[0];
      cell.original = rect;
      cell.placed = rect;
      cell.input_index = input_index++;
      model.cells.push_back(cell);
    } else if (tokens[5] == "MACRO" || tokens[5] == "BLOCKAGE") {
      Obstacle obstacle;
      obstacle.name = tokens[0];
      obstacle.rect = rect;
      obstacle.type = tokens[5] == "MACRO" ? ObstacleType::Macro : ObstacleType::Blockage;
      model.obstacles.push_back(obstacle);
    } else {
      return fail("line " + std::to_string(line_no) + ": unknown instance type '" + tokens[5] + "'");
    }
  }

  ParseResult result;
  result.ok = true;
  result.model = std::move(model);
  return result;
}

}  // namespace legalizer
