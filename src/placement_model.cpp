#include "placement_model.h"

#include <algorithm>
#include <cstdlib>

namespace legalizer {

Rect makeRect(Coord llx, Coord lly, Coord rect_width, Coord rect_height) {
  return Rect{llx, lly, llx + rect_width, lly + rect_height};
}

Coord width(const Rect &rect) { return rect.urx - rect.llx; }

Coord height(const Rect &rect) { return rect.ury - rect.lly; }

long long area(const Rect &rect) {
  if (!isValid(rect)) {
    return 0;
  }
  return width(rect) * height(rect);
}

bool isValid(const Rect &rect) {
  return rect.llx < rect.urx && rect.lly < rect.ury;
}

bool overlaps(const Rect &a, const Rect &b) {
  return a.llx < b.urx && b.llx < a.urx && a.lly < b.ury && b.lly < a.ury;
}

bool contains(const Rect &outer, const Rect &inner) {
  return outer.llx <= inner.llx && inner.urx <= outer.urx &&
         outer.lly <= inner.lly && inner.ury <= outer.ury;
}

Rect intersection(const Rect &a, const Rect &b) {
  Rect result{std::max(a.llx, b.llx), std::max(a.lly, b.lly),
              std::min(a.urx, b.urx), std::min(a.ury, b.ury)};
  if (!isValid(result)) {
    return Rect{};
  }
  return result;
}

Coord alignUp(Coord value, Coord origin, Coord step) {
  if (step <= 0) {
    return value;
  }
  Coord delta = value - origin;
  Coord rem = delta % step;
  if (rem < 0) {
    rem += step;
  }
  if (rem == 0) {
    return value;
  }
  return value + (step - rem);
}

Coord alignDown(Coord value, Coord origin, Coord step) {
  if (step <= 0) {
    return value;
  }
  Coord delta = value - origin;
  Coord rem = delta % step;
  if (rem < 0) {
    rem += step;
  }
  return value - rem;
}

Coord nearestAligned(Coord value, Coord origin, Coord step) {
  Coord down = alignDown(value, origin, step);
  Coord up = alignUp(value, origin, step);
  if (std::llabs(value - down) <= std::llabs(up - value)) {
    return down;
  }
  return up;
}

bool isAligned(Coord value, Coord origin, Coord step) {
  return step > 0 && alignDown(value, origin, step) == value;
}

bool isSiteAlignedX(const PlacementModel &model, Coord x) {
  return isAligned(x, model.die.llx, model.site_width);
}

bool isRowAlignedY(const PlacementModel &model, Coord y) {
  return isAligned(y, model.die.lly, model.site_height);
}

bool isSingleRowCell(const PlacementModel &model, const Cell &cell) {
  return height(cell.original) == model.site_height;
}

double dbuToMicron(const PlacementModel &model, Coord value) {
  return static_cast<double>(value) / static_cast<double>(model.dbu_per_micron);
}

double manhattanMicron(const PlacementModel &model, const Rect &a, const Rect &b) {
  Coord dist = std::llabs(a.llx - b.llx) + std::llabs(a.lly - b.lly);
  return dbuToMicron(model, dist);
}

Rect movedRect(const Rect &shape, Coord llx, Coord lly) {
  return makeRect(llx, lly, width(shape), height(shape));
}

}  // namespace legalizer
