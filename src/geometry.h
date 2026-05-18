#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>

using Coord = long long;

struct Point {
  Coord x = 0;
  Coord y = 0;
};

struct Rect {
  Coord llx = 0;
  Coord lly = 0;
  Coord urx = 0;
  Coord ury = 0;

  Coord width() const { return urx - llx; }
  Coord height() const { return ury - lly; }
};

struct Interval {
  Coord l = 0;
  Coord r = 0;
};

inline bool validRect(const Rect &r) { return r.llx < r.urx && r.lly < r.ury; }

inline bool overlaps(const Rect &a, const Rect &b) {
  return a.llx < b.urx && b.llx < a.urx && a.lly < b.ury && b.lly < a.ury;
}

inline bool contains(const Rect &outer, const Rect &inner) {
  return outer.llx <= inner.llx && inner.urx <= outer.urx && outer.lly <= inner.lly &&
         inner.ury <= outer.ury;
}

inline bool intervalOverlaps(const Interval &a, const Interval &b) {
  return a.l < b.r && b.l < a.r;
}

inline Coord floorDiv(Coord a, Coord b) {
  Coord q = a / b;
  Coord r = a % b;
  if (r != 0 && ((r < 0) != (b < 0))) {
    --q;
  }
  return q;
}

inline Coord ceilDiv(Coord a, Coord b) { return -floorDiv(-a, b); }

inline Coord snapDown(Coord value, Coord origin, Coord step) {
  return origin + floorDiv(value - origin, step) * step;
}

inline Coord snapUp(Coord value, Coord origin, Coord step) {
  return origin + ceilDiv(value - origin, step) * step;
}

inline bool aligned(Coord value, Coord origin, Coord step) {
  return step > 0 && (value - origin) % step == 0;
}

inline Coord manhattan(const Rect &a, Coord x, Coord y) {
  return std::llabs(a.llx - x) + std::llabs(a.lly - y);
}

inline Rect placedRect(const Rect &original, Coord x, Coord y) {
  return Rect{x, y, x + original.width(), y + original.height()};
}

