#include "placement_model.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace legalizer {

int64_t rectWidth(const Rect& rect) { return rect.x_max - rect.x_min; }

int64_t rectHeight(const Rect& rect) { return rect.y_max - rect.y_min; }

int64_t rectArea(const Rect& rect) {
  if (!isValidRect(rect)) return 0;
  const int64_t w = rectWidth(rect);
  const int64_t h = rectHeight(rect);
  if (w != 0 && h > std::numeric_limits<int64_t>::max() / w) {
    return std::numeric_limits<int64_t>::max();
  }
  return w * h;
}

bool isValidRect(const Rect& rect) {
  return rect.x_min < rect.x_max && rect.y_min < rect.y_max;
}

bool intersects(const Rect& a, const Rect& b) {
  return a.x_min < b.x_max && b.x_min < a.x_max && a.y_min < b.y_max &&
         b.y_min < a.y_max;
}

bool contains(const Rect& outer, const Rect& inner) {
  return outer.x_min <= inner.x_min && outer.y_min <= inner.y_min &&
         inner.x_max <= outer.x_max && inner.y_max <= outer.y_max;
}

Rect intersection(const Rect& a, const Rect& b) {
  Rect out{std::max(a.x_min, b.x_min), std::max(a.y_min, b.y_min),
           std::min(a.x_max, b.x_max), std::min(a.y_max, b.y_max)};
  if (!isValidRect(out)) return Rect{};
  return out;
}

int64_t floorDiv(int64_t value, int64_t divisor) {
  int64_t q = value / divisor;
  int64_t r = value % divisor;
  if (r != 0 && ((r < 0) != (divisor < 0))) --q;
  return q;
}

int64_t ceilDiv(int64_t value, int64_t divisor) {
  int64_t q = value / divisor;
  int64_t r = value % divisor;
  if (r != 0 && ((r > 0) == (divisor > 0))) ++q;
  return q;
}

int64_t snapDown(int64_t value, int64_t origin, int64_t pitch) {
  return origin + floorDiv(value - origin, pitch) * pitch;
}

int64_t snapUp(int64_t value, int64_t origin, int64_t pitch) {
  return origin + ceilDiv(value - origin, pitch) * pitch;
}

int64_t clampInt64(int64_t value, int64_t low, int64_t high) {
  return std::max(low, std::min(value, high));
}

Rect makeRect(int64_t x, int64_t y, int64_t width, int64_t height) {
  return Rect{x, y, x + width, y + height};
}

Rect movedRect(const Rect& original, int64_t x, int64_t y) {
  return makeRect(x, y, rectWidth(original), rectHeight(original));
}

int64_t manhattanDisplacement(const Rect& a, const Rect& b) {
  return std::llabs(a.x_min - b.x_min) + std::llabs(a.y_min - b.y_min);
}

bool validateDesign(const Design& design, std::string& error) {
  if (design.dbu_per_micron <= 0) {
    error = "DBU_Per_Micron must be positive";
    return false;
  }
  if (!isValidRect(design.die)) {
    error = "die area must have positive width and height";
    return false;
  }
  if (design.site_width <= 0 || design.site_height <= 0) {
    error = "site dimensions must be positive";
    return false;
  }
  if (rectHeight(design.die) < design.site_height) {
    error = "die height is smaller than one site row";
    return false;
  }

  std::unordered_set<std::string> names;
  for (const Cell& cell : design.cells) {
    if (cell.name.empty()) {
      error = "cell name cannot be empty";
      return false;
    }
    if (!names.insert(cell.name).second) {
      error = "duplicate movable cell name: " + cell.name;
      return false;
    }
    if (!isValidRect(cell.original)) {
      error = "cell has non-positive dimensions: " + cell.name;
      return false;
    }
    if (rectHeight(cell.original) % design.site_height != 0) {
      error = "cell height is not a multiple of site height: " + cell.name;
      return false;
    }
    if (rectHeight(cell.original) > rectHeight(design.die)) {
      error = "cell is taller than the die: " + cell.name;
      return false;
    }
  }

  for (const Obstacle& obstacle : design.obstacles) {
    if (!isValidRect(obstacle.rect)) {
      error = "obstacle has non-positive dimensions: " + obstacle.name;
      return false;
    }
  }

  return true;
}

}  // namespace legalizer
