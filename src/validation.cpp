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

