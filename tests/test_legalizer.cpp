#include "density_estimator.h"
#include "gp_parser.h"
#include "legalizer.h"
#include "row_interval_builder.h"
#include "tcl_writer.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace legalizer;

namespace {

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

PlacementModel tinyModel() {
  PlacementModel model;
  model.dbu_per_micron = 100;
  model.die = Rect{0, 0, 100, 20};
  model.site_width = 10;
  model.site_height = 10;
  Cell a;
  a.name = "a";
  a.original = makeRect(0, 0, 20, 10);
  a.placed = a.original;
  a.input_index = 0;
  model.cells.push_back(a);
  return model;
}

void testGeometry() {
  Rect a = makeRect(0, 0, 10, 10);
  Rect b = makeRect(10, 0, 10, 10);
  Rect c = makeRect(9, 0, 10, 10);
  require(!overlaps(a, b), "half-open adjacent rectangles must not overlap");
  require(overlaps(a, c), "intersecting rectangles must overlap");
  require(area(a) == 100, "area helper failed");
  require(alignUp(23, 0, 10) == 30, "alignUp failed");
  require(alignDown(23, 0, 10) == 20, "alignDown failed");
}

void testParser() {
  ParseResult parsed = parseGpFile("tests/fixture_one_cell.gp");
  require(parsed.ok, parsed.error);
  require(parsed.model.cells.size() == 1, "parser did not read one cell");
  require(parsed.model.cells[0].name == "u1", "parser lost cell name");
  require(width(parsed.model.cells[0].original) == 20, "parser width mismatch");

  ParseResult bad = parseGpFile("tests/fixture_malformed.gp");
  require(!bad.ok, "parser accepted negative dimension");
}

void testRowIntervals() {
  ParseResult parsed = parseGpFile("tests/fixture_obstacle.gp");
  require(parsed.ok, parsed.error);
  RowBuildResult rows = buildRowIntervals(parsed.model);
  require(rows.ok, rows.error);
  require(rows.rows.size() == 2, "row count mismatch");
  require(rows.rows[0].intervals.size() == 3, "macro/blockage should split first row");
  require(rows.rows[0].intervals[0].llx == 0 && rows.rows[0].intervals[0].urx == 50,
          "left interval mismatch");
  require(rows.rows[0].intervals[1].llx == 100 && rows.rows[0].intervals[1].urx == 150,
          "right interval mismatch");
  require(rows.rows[0].intervals[2].llx == 170 && rows.rows[0].intervals[2].urx == 200,
          "post-blockage interval mismatch");
}

void testOrdering() {
  PlacementModel model = tinyModel();
  Cell b;
  b.name = "b";
  b.original = makeRect(0, 10, 10, 10);
  b.placed = b.original;
  b.input_index = 1;
  model.cells.push_back(b);
  std::vector<int> order = cellOrder(model, false);
  require(order.size() == 2 && order[0] == 0 && order[1] == 1, "forward tie ordering failed");
  std::vector<int> rev = cellOrder(model, true);
  require(rev.size() == 2, "reverse ordering size failed");
}

void testLegalizationOverlap() {
  ParseResult parsed = parseGpFile("tests/fixture_overlap.gp");
  require(parsed.ok, parsed.error);
  RowBuildResult rows = buildRowIntervals(parsed.model);
  require(rows.ok, rows.error);
  LegalizeOptions opts;
  opts.alpha = 0.7;
  opts.threshold = 45.0;
  LegalizeResult result = legalizePlacement(parsed.model, rows.rows, opts);
  require(result.ok, result.error);
  ValidationResult validation = validatePlacement(result.model, rows.rows, opts);
  require(validation.ok, validation.error);
  require(!overlaps(result.model.cells[0].placed, result.model.cells[1].placed),
          "legalizer left cell overlap");
}

void testObstacleLegalization() {
  ParseResult parsed = parseGpFile("tests/fixture_obstacle.gp");
  require(parsed.ok, parsed.error);
  RowBuildResult rows = buildRowIntervals(parsed.model);
  require(rows.ok, rows.error);
  LegalizeOptions opts;
  LegalizeResult result = legalizePlacement(parsed.model, rows.rows, opts);
  require(result.ok, result.error);
  ValidationResult validation = validatePlacement(result.model, rows.rows, opts);
  require(validation.ok, validation.error);
}

void testDensity() {
  PlacementModel model = tinyModel();
  model.cells[0].placed = makeRect(0, 0, 100, 10);
  model.cells[0].has_placement = true;
  DensityResult density = computeFinalDensity(model, 45.0);
  require(density.total_grids == 1, "density grid count mismatch");
  require(density.overflow_grids == 1, "density overflow mismatch");

  Obstacle macro;
  macro.name = "macro";
  macro.rect = makeRect(0, 0, 50, 10);
  macro.type = ObstacleType::Macro;
  model.obstacles.push_back(macro);
  density = computeFinalDensity(model, 45.0);
  require(density.total_grids == 0, "macro-covered grid should be excluded");
}

void testWriter() {
  PlacementModel model = tinyModel();
  model.cells[0].placed = makeRect(20, 0, 20, 10);
  model.cells[0].has_placement = true;
  WriteResult write = writePlacementTcl(model, "tests/out_writer.tcl");
  require(write.ok, write.error);
  std::ifstream in("tests/out_writer.tcl");
  std::stringstream buffer;
  buffer << in.rdbuf();
  std::string text = buffer.str();
  require(text.find("place_cell -inst_name {a} -orient R0 -origin {0.2 0}") != std::string::npos,
          "writer command mismatch");
  require(text.find("detailed_placement") == std::string::npos,
          "writer emitted forbidden detailed_placement");
}

void testEndToEndOneCell() {
  ParseResult parsed = parseGpFile("tests/fixture_one_cell.gp");
  require(parsed.ok, parsed.error);
  RowBuildResult rows = buildRowIntervals(parsed.model);
  require(rows.ok, rows.error);
  LegalizeOptions opts;
  LegalizeResult result = legalizePlacement(parsed.model, rows.rows, opts);
  require(result.ok, result.error);
  require(result.model.cells[0].placed.llx == 20 || result.model.cells[0].placed.llx == 30,
          "one cell was not snapped near original x");
}

}  // namespace

int main() {
  try {
    testGeometry();
    testParser();
    testRowIntervals();
    testOrdering();
    testLegalizationOverlap();
    testObstacleLegalization();
    testDensity();
    testWriter();
    testEndToEndOneCell();
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
  }
  std::cout << "All tests passed\n";
  return 0;
}
