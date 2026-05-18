#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "density_estimator.hpp"
#include "gp_parser.hpp"
#include "legalizer.hpp"
#include "placement_model.hpp"
#include "row_interval_builder.hpp"
#include "tcl_writer.hpp"

using namespace legalizer;

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    std::exit(1);
  }
}

double displacementUm(const Design& d, const Cell& cell) {
  return static_cast<double>(manhattanDisplacement(cell.original, cell.placed)) /
         static_cast<double>(d.dbu_per_micron);
}

struct PlacementMetrics {
  double total_displacement = 0.0;
  double average_displacement = 0.0;
  double max_displacement = 0.0;
  double density_proxy = 0.0;
};

PlacementMetrics computeMetrics(const Design& d, double threshold) {
  PlacementMetrics metrics;
  size_t placed_count = 0;
  for (const Cell& cell : d.cells) {
    if (!cell.has_placement) continue;
    const double disp = displacementUm(d, cell);
    metrics.total_displacement += disp;
    metrics.max_displacement = std::max(metrics.max_displacement, disp);
    ++placed_count;
  }
  if (placed_count > 0) {
    metrics.average_displacement =
        metrics.total_displacement / static_cast<double>(placed_count);
  }

  DensityEstimator density(d, threshold);
  density.rebuildMovableOccupancy();
  metrics.density_proxy = density.overflowProxy();
  return metrics;
}

void requireLegalPlacement(const Design& d) {
  for (const Cell& cell : d.cells) {
    require(cell.has_placement, "cell has placement: " + cell.name);
    require(contains(d.die, cell.placed), "cell inside die: " + cell.name);
    require((cell.placed.x_min - d.die.x_min) % d.site_width == 0,
            "cell x aligned: " + cell.name);
    require((cell.placed.y_min - d.die.y_min) % d.site_height == 0,
            "cell y aligned: " + cell.name);
    for (const Obstacle& obstacle : d.obstacles) {
      require(!intersects(cell.placed, obstacle.rect),
              "cell avoids obstacle: " + cell.name);
    }
  }
  for (size_t i = 0; i < d.cells.size(); ++i) {
    for (size_t j = i + 1; j < d.cells.size(); ++j) {
      require(!intersects(d.cells[i].placed, d.cells[j].placed),
              "movable cells do not overlap");
    }
  }
}

Design tinyDesign() {
  Design d;
  d.dbu_per_micron = 1000;
  d.die = Rect{0, 0, 10000, 4000};
  d.site_width = 100;
  d.site_height = 1000;
  return d;
}

Cell makeCell(const std::string& name, int64_t x, int64_t y, int64_t w,
              int64_t h, size_t index) {
  Cell c;
  c.name = name;
  c.original = makeRect(x, y, w, h);
  c.placed = c.original;
  c.input_index = index;
  return c;
}

Obstacle makeObstacle(const std::string& name, int64_t x, int64_t y, int64_t w,
                      int64_t h, InstanceType type) {
  Obstacle o;
  o.name = name;
  o.rect = makeRect(x, y, w, h);
  o.type = type;
  return o;
}

void testGeometry() {
  Rect a{0, 0, 10, 10};
  Rect b{10, 0, 20, 10};
  Rect c{9, 0, 20, 10};
  require(!intersects(a, b), "half-open edge touch must not overlap");
  require(intersects(a, c), "interior intersection must overlap");
  require(contains(Rect{0, 0, 20, 20}, a), "contains valid child rect");
  require(rectWidth(intersection(a, c)) == 1, "intersection width");
  require(snapUp(101, 0, 100) == 200, "snap up");
  require(snapDown(199, 0, 100) == 100, "snap down");
}

void testParser() {
  Design design;
  std::string error;
  require(GpParser::parseFile("tests/fixture_one_cell.gp", design, error),
          "valid fixture parses: " + error);
  require(design.cells.size() == 1, "parser sees one cell");
  require(design.cells[0].name == "u0", "parser preserves name");
  require(design.site_height == 1000, "parser reads site height");

  require(!GpParser::parseFile("tests/fixture_bad_type.gp", design, error),
          "unknown type fails");
}

void testRows() {
  Design d = tinyDesign();
  d.obstacles.push_back(
      makeObstacle("m0", 3000, 0, 2000, 1000, InstanceType::Macro));
  std::vector<LegalRow> rows;
  std::string error;
  require(RowIntervalBuilder::build(d, rows, error), "rows build: " + error);
  require(rows.size() == 4, "row count");
  require(rows[0].free_intervals.size() == 2, "macro splits first row");
  require(rows[0].free_intervals[0].x_max == 3000, "left interval ends at macro");
  require(rows[0].free_intervals[1].x_min == 5000,
          "right interval starts after macro");
  require(rows[1].free_intervals.size() == 1, "unaffected row remains whole");
}

void testDensity() {
  Design d = tinyDesign();
  d.die = Rect{0, 0, 20000, 10000};
  d.obstacles.push_back(
      makeObstacle("h0", 0, 0, 10000, 10000, InstanceType::Macro));
  DensityEstimator density(d, 10.0);
  require(density.gridSize() == 10000, "10 micron grid size");
  require(density.scoreCandidate(makeRect(0, 0, 10000, 10000)) == 0.0,
          "fully macro-covered grid excluded");
  Rect r = makeRect(10000, 0, 1000, 1000);
  const double before = density.scoreCandidate(r);
  density.commit(r);
  const double after = density.scoreCandidate(r);
  require(after >= before, "committed occupancy increases future score");
  require(density.overflowProxy() >= 0.0, "density overflow proxy available");
}

void testLegalizerAndWriter() {
  Design d = tinyDesign();
  d.cells.push_back(makeCell("u0", 123, 99, 200, 1000, 0));
  d.cells.push_back(makeCell("u1", 124, 100, 200, 1000, 1));
  d.obstacles.push_back(
      makeObstacle("b0", 0, 0, 100, 4000, InstanceType::Blockage));
  std::string error;
  require(validateDesign(d, error), "model validates: " + error);

  std::vector<LegalRow> rows;
  require(RowIntervalBuilder::build(d, rows, error), "row build: " + error);
  DensityEstimator density(d, 45.0);
  Legalizer legalizer(d, rows, density, 0.7);
  require(legalizer.legalize(error), "legalize succeeds: " + error);
  requireLegalPlacement(d);

  require(TclWriter::writeFile("tests/out_writer.tcl", d, error),
          "writer succeeds: " + error);
  std::ifstream in("tests/out_writer.tcl");
  std::string text((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  require(text.find("place_cell -inst_name u0 -orient R0 -origin {") !=
              std::string::npos,
          "writer command shape");
  require(text.find("detailed_placement") == std::string::npos,
          "writer never emits detailed placement");
}

void testConstrainedCellAvoidsTailDisplacement() {
  Design d;
  d.dbu_per_micron = 1000;
  d.die = Rect{0, 0, 2000, 2000};
  d.site_width = 100;
  d.site_height = 1000;
  d.cells.push_back(makeCell("flex", 0, 0, 1000, 1000, 0));
  d.cells.push_back(makeCell("tight", 0, 0, 100, 2000, 1));
  d.obstacles.push_back(
      makeObstacle("row0_block", 100, 0, 1900, 1000, InstanceType::Blockage));

  std::string error;
  require(validateDesign(d, error), "tail fixture validates: " + error);
  std::vector<LegalRow> rows;
  require(RowIntervalBuilder::build(d, rows, error), "tail rows build: " + error);
  DensityEstimator density(d, 45.0);
  Legalizer legalizer(d, rows, density, 0.7);
  require(legalizer.legalize(error), "tail legalize succeeds: " + error);
  requireLegalPlacement(d);

  const Cell& tight = d.cells[1];
  require(tight.placed.x_min == 0 && tight.placed.y_min == 0,
          "constrained multi-row cell keeps its only near-origin slot");
  double max_disp = 0.0;
  for (const Cell& cell : d.cells) {
    max_disp = std::max(max_disp, displacementUm(d, cell));
  }
  require(max_disp <= 1.2, "tail displacement remains bounded");
}

void testRepairKeepsDensityAwarePlacement() {
  Design d;
  d.dbu_per_micron = 1000;
  d.die = Rect{0, 0, 20000, 1000};
  d.site_width = 100;
  d.site_height = 1000;
  d.cells.push_back(makeCell("u0", 0, 0, 100, 1000, 0));
  d.cells.push_back(makeCell("u1", 0, 0, 100, 1000, 1));

  std::string error;
  require(validateDesign(d, error), "density repair fixture validates: " + error);
  std::vector<LegalRow> rows;
  require(RowIntervalBuilder::build(d, rows, error),
          "density repair rows build: " + error);
  DensityEstimator density(d, 1.0);
  Legalizer legalizer(d, rows, density, 0.0);
  require(legalizer.legalize(error), "density repair legalize succeeds: " + error);
  requireLegalPlacement(d);

  const PlacementMetrics metrics = computeMetrics(d, 1.0);
  require(metrics.density_proxy == 0.0,
          "repair does not increase density overflow proxy");
  require(d.cells[1].placed.x_min >= 10000,
          "repair keeps density-aware second-grid placement");
  require(metrics.max_displacement >= 10.0,
          "large displacement is kept when repair would worsen density");
}

void testRepairReducesMaxWhenDensityProxyStable() {
  Design d;
  d.dbu_per_micron = 1000;
  d.die = Rect{0, 0, 20000, 1000};
  d.site_width = 100;
  d.site_height = 1000;
  d.cells.push_back(makeCell("u0", 0, 0, 200, 1000, 0));
  d.cells.push_back(makeCell("u1", 0, 0, 100, 1000, 1));

  std::string error;
  require(validateDesign(d, error), "max repair fixture validates: " + error);
  std::vector<LegalRow> rows;
  require(RowIntervalBuilder::build(d, rows, error),
          "max repair rows build: " + error);
  DensityEstimator density(d, 1.0);
  Legalizer legalizer(d, rows, density, 0.0);
  require(legalizer.legalize(error), "max repair legalize succeeds: " + error);
  requireLegalPlacement(d);

  const PlacementMetrics metrics = computeMetrics(d, 1.0);
  require(metrics.density_proxy == 50.0,
          "repair keeps existing overflow-grid count stable");
  require(d.cells[1].placed.x_min < 10000,
          "repair moves tail cell back when DOR proxy is unchanged");
  require(metrics.max_displacement < 1.0,
          "repair meaningfully reduces maximum displacement");
}

void testOverfullFailure() {
  Design d = tinyDesign();
  d.die = Rect{0, 0, 300, 1000};
  d.cells.push_back(makeCell("u0", 0, 0, 200, 1000, 0));
  d.cells.push_back(makeCell("u1", 0, 0, 200, 1000, 1));
  std::string error;
  std::vector<LegalRow> rows;
  require(RowIntervalBuilder::build(d, rows, error), "small row build");
  DensityEstimator density(d, 45.0);
  Legalizer legalizer(d, rows, density, 0.7);
  require(!legalizer.legalize(error), "overfull design fails");
  require(error.find("cannot place cell") != std::string::npos,
          "overfull error names placement failure");
}

}  // namespace

int main() {
  testGeometry();
  testParser();
  testRows();
  testDensity();
  testLegalizerAndWriter();
  testConstrainedCellAvoidsTailDisplacement();
  testRepairKeepsDensityAwarePlacement();
  testRepairReducesMaxWhenDensityProxyStable();
  testOverfullFailure();
  std::cout << "All tests passed\n";
  return 0;
}
