#pragma once

#include "geometry.h"

#include <string>
#include <vector>

enum class ObjectType { Cell, Macro, Blockage };

struct Cell {
  std::string name;
  Rect original;
  std::size_t inputIndex = 0;
  bool placed = false;
  Coord x = 0;
  Coord y = 0;
};

struct Obstacle {
  std::string name;
  Rect rect;
  ObjectType type = ObjectType::Macro;
  std::size_t inputIndex = 0;
};

struct Design {
  int dbuPerMicron = 0;
  Rect die;
  Coord siteWidth = 0;
  Coord siteHeight = 0;
  std::vector<Cell> cells;
  std::vector<Obstacle> obstacles;
};

inline double dbuToMicron(Coord value, int dbuPerMicron) {
  return static_cast<double>(value) / static_cast<double>(dbuPerMicron);
}

