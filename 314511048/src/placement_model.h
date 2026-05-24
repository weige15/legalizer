#ifndef PLACEMENT_MODEL_H
#define PLACEMENT_MODEL_H

#include <string>
#include <vector>

namespace legalizer {

using Dbu = long long;

struct Status {
  bool ok = true;
  std::string message;

  static Status Ok() { return Status{true, ""}; }
  static Status Error(const std::string &msg) { return Status{false, msg}; }
};

struct Point {
  Dbu x = 0;
  Dbu y = 0;
};

struct Rect {
  Dbu lx = 0;
  Dbu ly = 0;
  Dbu ux = 0;
  Dbu uy = 0;
};

enum class ObjectType {
  Cell,
  Macro,
  Blockage,
};

struct Cell {
  std::string name;
  Point original;
  Point placed;
  Dbu width = 0;
  Dbu height = 0;
  std::string orient;
  bool placedValid = false;
  std::string originalOrient;
};

struct Obstacle {
  std::string name;
  Rect rect;
  ObjectType type = ObjectType::Macro;
  std::string orient;
};

struct Tech {
  int dbuPerMicron = 0;
  Rect die;
  Dbu siteWidth = 0;
  Dbu siteHeight = 0;
};

struct PlacementModel {
  Tech tech;
  std::vector<Cell> cells;
  std::vector<Obstacle> obstacles;
};

bool overlaps(const Rect &a, const Rect &b);
bool contains(const Rect &outer, const Rect &inner);
Dbu rectArea(const Rect &r);
Rect rectForCell(const Cell &cell, Point origin);
Rect rectForPlacedCell(const Cell &cell);
double dbuToMicron(const Tech &tech, Dbu value);
Dbu micronToDbu(const Tech &tech, double value);
Dbu floorDiv(Dbu a, Dbu b);
Dbu ceilDiv(Dbu a, Dbu b);
Dbu snapDownToSite(const Tech &tech, Dbu x);
Dbu snapUpToSite(const Tech &tech, Dbu x);
Dbu snapNearestToSite(const Tech &tech, Dbu x);
Dbu clampSiteOrigin(const Tech &tech, Dbu x, Dbu minX, Dbu maxOrigin);
bool isSiteAligned(const Tech &tech, Dbu x);
bool isRowAligned(const Tech &tech, Dbu y);
int rowIndexForY(const Tech &tech, Dbu y);
int nearestRowIndexForY(const Tech &tech, Dbu y);
int rowCount(const Tech &tech);
Dbu rowY(const Tech &tech, int rowIndex);
Status validateTech(const Tech &tech);
Status validateSupportedCells(const PlacementModel &model);
bool isSupportedCell(const Tech &tech, const Cell &cell, std::string *why);

}  // namespace legalizer

#endif
