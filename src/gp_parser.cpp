#include "gp_parser.hpp"

#include <fstream>
#include <sstream>
#include <vector>

namespace legalizer {
namespace {

bool parseInt64(const std::string& token, int64_t& value) {
  if (token.empty()) return false;
  size_t idx = 0;
  try {
    value = std::stoll(token, &idx, 10);
  } catch (...) {
    return false;
  }
  return idx == token.size();
}

std::vector<std::string> splitTokens(const std::string& line) {
  std::istringstream iss(line);
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token) tokens.push_back(token);
  return tokens;
}

bool readMetadataLine(std::istream& in, int& line_no, const std::string& key,
                      std::vector<int64_t>& values, std::string& error) {
  std::string line;
  if (!std::getline(in, line)) {
    error = "missing metadata line: " + key;
    return false;
  }
  ++line_no;
  const auto tokens = splitTokens(line);
  if (tokens.empty() || tokens[0] != key) {
    error = "line " + std::to_string(line_no) + ": expected " + key;
    return false;
  }
  if (tokens.size() != values.size() + 1) {
    error = "line " + std::to_string(line_no) + ": wrong field count for " + key;
    return false;
  }
  for (size_t i = 0; i < values.size(); ++i) {
    if (!parseInt64(tokens[i + 1], values[i])) {
      error = "line " + std::to_string(line_no) + ": invalid integer '" +
              tokens[i + 1] + "'";
      return false;
    }
  }
  return true;
}

}  // namespace

bool GpParser::parseFile(const std::string& path, Design& design,
                         std::string& error) {
  std::ifstream in(path);
  if (!in) {
    error = "cannot open input file: " + path;
    return false;
  }

  Design parsed;
  int line_no = 0;

  std::vector<int64_t> one(1);
  std::vector<int64_t> two(2);
  if (!readMetadataLine(in, line_no, "DBU_Per_Micron", one, error)) return false;
  parsed.dbu_per_micron = one[0];
  if (!readMetadataLine(in, line_no, "DieArea_LL", two, error)) return false;
  parsed.die.x_min = two[0];
  parsed.die.y_min = two[1];
  if (!readMetadataLine(in, line_no, "DieArea_UR", two, error)) return false;
  parsed.die.x_max = two[0];
  parsed.die.y_max = two[1];
  if (!readMetadataLine(in, line_no, "Site_Width", one, error)) return false;
  parsed.site_width = one[0];
  if (!readMetadataLine(in, line_no, "Site_Height", one, error)) return false;
  parsed.site_height = one[0];

  std::string line;
  bool found_header = false;
  while (std::getline(in, line)) {
    ++line_no;
    const auto tokens = splitTokens(line);
    if (tokens.empty()) continue;
    if (tokens.size() == 6 && tokens[0] == "Name" && tokens[1] == "LLX" &&
        tokens[2] == "LLY" && tokens[3] == "Width" &&
        tokens[4] == "Height" && tokens[5] == "Type") {
      found_header = true;
      break;
    }
    error = "line " + std::to_string(line_no) + ": expected instance header";
    return false;
  }
  if (!found_header) {
    error = "missing instance header";
    return false;
  }

  while (std::getline(in, line)) {
    ++line_no;
    const auto tokens = splitTokens(line);
    if (tokens.empty()) continue;
    if (tokens.size() != 6) {
      error = "line " + std::to_string(line_no) + ": wrong instance field count";
      return false;
    }

    int64_t x = 0, y = 0, w = 0, h = 0;
    if (!parseInt64(tokens[1], x) || !parseInt64(tokens[2], y) ||
        !parseInt64(tokens[3], w) || !parseInt64(tokens[4], h)) {
      error = "line " + std::to_string(line_no) + ": invalid integer field";
      return false;
    }
    if (w <= 0 || h <= 0) {
      error = "line " + std::to_string(line_no) +
              ": instance dimensions must be positive";
      return false;
    }

    const Rect rect = makeRect(x, y, w, h);
    if (tokens[5] == "CELL") {
      Cell cell;
      cell.name = tokens[0];
      cell.original = rect;
      cell.placed = rect;
      cell.input_index = parsed.cells.size();
      parsed.cells.push_back(cell);
    } else if (tokens[5] == "MACRO" || tokens[5] == "BLOCKAGE") {
      Obstacle obstacle;
      obstacle.name = tokens[0];
      obstacle.rect = rect;
      obstacle.type =
          (tokens[5] == "MACRO") ? InstanceType::Macro : InstanceType::Blockage;
      parsed.obstacles.push_back(obstacle);
    } else {
      error = "line " + std::to_string(line_no) + ": unknown instance type '" +
              tokens[5] + "'";
      return false;
    }
  }

  if (!validateDesign(parsed, error)) {
    error = "model validation failed: " + error;
    return false;
  }

  design = std::move(parsed);
  return true;
}

}  // namespace legalizer
