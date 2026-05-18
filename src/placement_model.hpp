#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace legalizer {

struct Rect {
  int64_t x_min = 0;
  int64_t y_min = 0;
  int64_t x_max = 0;
  int64_t y_max = 0;
};

enum class InstanceType {
  Cell,
  Macro,
  Blockage,
};

struct Cell {
  std::string name;
  Rect original;
  Rect placed;
  bool has_placement = false;
  size_t input_index = 0;
};

struct Obstacle {
  std::string name;
  Rect rect;
  InstanceType type = InstanceType::Macro;
};

struct Design {
  int64_t dbu_per_micron = 0;
  Rect die;
  int64_t site_width = 0;
  int64_t site_height = 0;
  std::vector<Cell> cells;
  std::vector<Obstacle> obstacles;
};

int64_t rectWidth(const Rect& rect);
int64_t rectHeight(const Rect& rect);
int64_t rectArea(const Rect& rect);
bool isValidRect(const Rect& rect);
bool intersects(const Rect& a, const Rect& b);
bool contains(const Rect& outer, const Rect& inner);
Rect intersection(const Rect& a, const Rect& b);

int64_t floorDiv(int64_t value, int64_t divisor);
int64_t ceilDiv(int64_t value, int64_t divisor);
int64_t snapDown(int64_t value, int64_t origin, int64_t pitch);
int64_t snapUp(int64_t value, int64_t origin, int64_t pitch);
int64_t clampInt64(int64_t value, int64_t low, int64_t high);

Rect makeRect(int64_t x, int64_t y, int64_t width, int64_t height);
Rect movedRect(const Rect& original, int64_t x, int64_t y);
int64_t manhattanDisplacement(const Rect& a, const Rect& b);

bool validateDesign(const Design& design, std::string& error);

}  // namespace legalizer
