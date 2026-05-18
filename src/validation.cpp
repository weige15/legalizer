#include "validation.h"

#include <algorithm>
#include <sstream>
#include <vector>

bool validateDesign(const Design &design, const RowModel &baseRows, std::string *error) {
  RowModel checker(design);
  for (const Cell &cell : design.cells) {
    if (!cell.placed) {
      if (error) {
        *error = "cell " + cell.name + " is missing a placement";
      }
      return false;
    }
    const Rect pr = placedRect(cell.original, cell.x, cell.y);
    if (!contains(design.die, pr)) {
      if (error) {
        *error = "cell " + cell.name + " is outside the die";
      }
      return false;
    }
    if (checker.rowIndexForY(cell.y) < 0 || !aligned(cell.x, design.die.llx, design.siteWidth)) {
      if (error) {
        *error = "cell " + cell.name + " is not aligned to a legal site";
      }
      return false;
    }
    for (const Obstacle &obs : design.obstacles) {
      if (overlaps(pr, obs.rect)) {
        if (error) {
          *error = "cell " + cell.name + " overlaps fixed object " + obs.name;
        }
        return false;
      }
    }
    if (!checker.commit(cell, cell.x, cell.y)) {
      if (error) {
        std::ostringstream oss;
        oss << "illegal placement for cell " << cell.name << " at " << cell.x << "," << cell.y;
        *error = oss.str();
      }
      return false;
    }
  }
  (void)baseRows;
  return true;
}
