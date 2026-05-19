#ifndef PLACEMENT_MODEL_H
#define PLACEMENT_MODEL_H

#include <cstdint>
#include <string>
#include <vector>

namespace legalizer {

using Coord = long long;

struct Rect {
  Coord llx = 0;
  Coord lly = 0;
  Coord urx = 0;
  Coord ury = 0;
};

enum class ObstacleType {
  Macro,
  Blockage,
};

struct Cell {
  std::string name;
  Rect original;
  Rect placed;
  bool has_placement = false;
  std::size_t input_index = 0;
};

struct Obstacle {
  std::string name;
  Rect rect;
  ObstacleType type = ObstacleType::Macro;
};

struct PlacementModel {
  int dbu_per_micron = 0;
  Rect die;
  Coord site_width = 0;
  Coord site_height = 0;
  std::vector<Cell> cells;
  std::vector<Obstacle> obstacles;
};

Rect makeRect(Coord llx, Coord lly, Coord width, Coord height);
Coord width(const Rect &rect);
Coord height(const Rect &rect);
long long area(const Rect &rect);
bool isValid(const Rect &rect);
bool overlaps(const Rect &a, const Rect &b);
bool contains(const Rect &outer, const Rect &inner);
Rect intersection(const Rect &a, const Rect &b);
Coord alignUp(Coord value, Coord origin, Coord step);
Coord alignDown(Coord value, Coord origin, Coord step);
Coord nearestAligned(Coord value, Coord origin, Coord step);
bool isAligned(Coord value, Coord origin, Coord step);
bool isSiteAlignedX(const PlacementModel &model, Coord x);
bool isRowAlignedY(const PlacementModel &model, Coord y);
bool isSingleRowCell(const PlacementModel &model, const Cell &cell);
double dbuToMicron(const PlacementModel &model, Coord value);
double manhattanMicron(const PlacementModel &model, const Rect &a, const Rect &b);
Rect movedRect(const Rect &shape, Coord llx, Coord lly);

}  // namespace legalizer

#endif
