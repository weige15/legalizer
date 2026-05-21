#include "placement_model.h"

#include <cmath>
#include <limits>

namespace legalizer {

bool overlaps(const Rect &a, const Rect &b) {
  return a.lx < b.ux && b.lx < a.ux && a.ly < b.uy && b.ly < a.uy;
}

bool contains(const Rect &outer, const Rect &inner) {
  return outer.lx <= inner.lx && inner.ux <= outer.ux &&
         outer.ly <= inner.ly && inner.uy <= outer.uy;
}

Dbu rectArea(const Rect &r) {
  if (r.ux <= r.lx || r.uy <= r.ly) {
    return 0;
  }
  return (r.ux - r.lx) * (r.uy - r.ly);
}

Rect rectForCell(const Cell &cell, Point origin) {
  return Rect{origin.x, origin.y, origin.x + cell.width, origin.y + cell.height};
}

Rect rectForPlacedCell(const Cell &cell) {
  return rectForCell(cell, cell.placed);
}

double dbuToMicron(const Tech &tech, Dbu value) {
  return static_cast<double>(value) / static_cast<double>(tech.dbuPerMicron);
}

Dbu micronToDbu(const Tech &tech, double value) {
  return static_cast<Dbu>(std::llround(value * static_cast<double>(tech.dbuPerMicron)));
}

Dbu floorDiv(Dbu a, Dbu b) {
  Dbu q = a / b;
  Dbu r = a % b;
  if (r != 0 && ((r > 0) != (b > 0))) {
    --q;
  }
  return q;
}

Dbu ceilDiv(Dbu a, Dbu b) {
  return -floorDiv(-a, b);
}

Dbu snapDownToSite(const Tech &tech, Dbu x) {
  Dbu offset = x - tech.die.lx;
  return tech.die.lx + floorDiv(offset, tech.siteWidth) * tech.siteWidth;
}

Dbu snapUpToSite(const Tech &tech, Dbu x) {
  Dbu offset = x - tech.die.lx;
  return tech.die.lx + ceilDiv(offset, tech.siteWidth) * tech.siteWidth;
}

Dbu snapNearestToSite(const Tech &tech, Dbu x) {
  Dbu down = snapDownToSite(tech, x);
  Dbu up = snapUpToSite(tech, x);
  if (std::llabs(x - down) <= std::llabs(up - x)) {
    return down;
  }
  return up;
}

Dbu clampSiteOrigin(const Tech &tech, Dbu x, Dbu minX, Dbu maxOrigin) {
  if (maxOrigin < minX) {
    return minX;
  }
  Dbu snapped = snapNearestToSite(tech, x);
  if (snapped < minX) {
    return snapUpToSite(tech, minX);
  }
  if (snapped > maxOrigin) {
    return snapDownToSite(tech, maxOrigin);
  }
  return snapped;
}

bool isSiteAligned(const Tech &tech, Dbu x) {
  return tech.siteWidth > 0 && (x - tech.die.lx) % tech.siteWidth == 0;
}

bool isRowAligned(const Tech &tech, Dbu y) {
  if (tech.siteHeight <= 0) {
    return false;
  }
  if ((y - tech.die.ly) % tech.siteHeight != 0) {
    return false;
  }
  int idx = rowIndexForY(tech, y);
  return idx >= 0;
}

int rowCount(const Tech &tech) {
  if (tech.siteHeight <= 0 || tech.die.uy <= tech.die.ly) {
    return 0;
  }
  return static_cast<int>((tech.die.uy - tech.die.ly) / tech.siteHeight);
}

Dbu rowY(const Tech &tech, int rowIndex) {
  return tech.die.ly + static_cast<Dbu>(rowIndex) * tech.siteHeight;
}

int rowIndexForY(const Tech &tech, Dbu y) {
  if (tech.siteHeight <= 0 || y < tech.die.ly) {
    return -1;
  }
  Dbu delta = y - tech.die.ly;
  if (delta % tech.siteHeight != 0) {
    return -1;
  }
  int idx = static_cast<int>(delta / tech.siteHeight);
  if (idx < 0 || idx >= rowCount(tech)) {
    return -1;
  }
  return idx;
}

int nearestRowIndexForY(const Tech &tech, Dbu y) {
  int rows = rowCount(tech);
  if (rows <= 0) {
    return -1;
  }
  double rel = static_cast<double>(y - tech.die.ly) / static_cast<double>(tech.siteHeight);
  int idx = static_cast<int>(std::llround(rel));
  if (idx < 0) {
    return 0;
  }
  if (idx >= rows) {
    return rows - 1;
  }
  return idx;
}

Status validateTech(const Tech &tech) {
  if (tech.dbuPerMicron <= 0) {
    return Status::Error("DBU_Per_Micron must be positive");
  }
  if (tech.die.ux <= tech.die.lx || tech.die.uy <= tech.die.ly) {
    return Status::Error("die upper bounds must exceed lower bounds");
  }
  if (tech.siteWidth <= 0 || tech.siteHeight <= 0) {
    return Status::Error("site dimensions must be positive");
  }
  if (rowCount(tech) <= 0) {
    return Status::Error("die contains no complete placement rows");
  }
  return Status::Ok();
}

bool isSupportedCell(const Tech &tech, const Cell &cell, std::string *why) {
  if (cell.width <= 0 || cell.height <= 0) {
    if (why) *why = "nonpositive dimensions";
    return false;
  }
  if (cell.height != tech.siteHeight) {
    if (why) *why = "multi-row movable cell";
    return false;
  }
  if (cell.width % tech.siteWidth != 0) {
    if (why) *why = "width is not a site multiple";
    return false;
  }
  return true;
}

Status validateSupportedCells(const PlacementModel &model) {
  for (const Cell &cell : model.cells) {
    std::string why;
    if (!isSupportedCell(model.tech, cell, &why)) {
      return Status::Error("unsupported CELL '" + cell.name + "': " + why);
    }
  }
  return Status::Ok();
}

}  // namespace legalizer
