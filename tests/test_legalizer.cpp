#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "density_estimator.h"
#include "gp_parser.h"
#include "legalizer.h"
#include "row_interval_builder.h"
#include "tcl_writer.h"

using namespace legalizer;

namespace {

int failures = 0;
std::string currentTest;

void fail(const std::string &message) {
  ++failures;
  std::cerr << "[FAIL] " << currentTest << ": " << message << "\n";
}

void expectTrue(bool value, const std::string &message) {
  if (!value) {
    fail(message);
  }
}

template <typename T>
void expectEq(const T &actual, const T &expected, const std::string &label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << " expected " << expected << " actual " << actual;
    fail(out.str());
  }
}

void writeFile(const std::string &path, const std::string &text) {
  std::ofstream out(path);
  out << text;
}

std::string readFile(const std::string &path) {
  std::ifstream in(path);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

PlacementModel smallModel() {
  PlacementModel model;
  model.tech.dbuPerMicron = 1000;
  model.tech.die = Rect{0, 0, 10000, 3000};
  model.tech.siteWidth = 100;
  model.tech.siteHeight = 1000;
  return model;
}

Cell makeCell(const std::string &name, Point original, Dbu width, Dbu height,
              const std::string &orient, bool placedValid = false,
              Point placed = Point{}) {
  Cell cell;
  cell.name = name;
  cell.original = original;
  cell.placed = placedValid ? placed : Point{};
  cell.width = width;
  cell.height = height;
  cell.orient = orient;
  cell.placedValid = placedValid;
  cell.originalOrient = orient;
  return cell;
}

void run(const std::string &name, void (*fn)()) {
  currentTest = name;
  fn();
}

void testParserValidAndInvalid() {
  writeFile("tests/fixture_parser_valid.gp",
            "DBU_Per_Micron 1000\n"
            "DieArea_LL 0 0\n"
            "DieArea_UR 10000 2000\n"
            "Site_Width 100\n"
            "Site_Height 1000\n\n"
            "Name LLX LLY Width Height Orient Type\n"
            "u0 0 0 100 1000 R0 CELL\n"
            "m0 1000 0 500 1000 R0 MACRO\n"
            "b0 2000 0 500 1000 BLOCKAGE\n");
  PlacementModel model;
  Status status = parseGpFile("tests/fixture_parser_valid.gp", &model);
  expectTrue(status.ok, status.message);
  expectEq(model.cells.size(), static_cast<std::size_t>(1), "cell count");
  expectEq(model.obstacles.size(), static_cast<std::size_t>(2), "obstacle count");
  expectEq(model.cells[0].orient, std::string("R0"), "orientation");

  writeFile("tests/fixture_parser_bad.gp",
            "DBU_Per_Micron 1000\n"
            "DieArea_LL 0 0\n"
            "DieArea_UR 10000 2000\n"
            "Site_Width 100\n"
            "Site_Height 1000\n\n"
            "Name LLX LLY Width Height Orient Type\n"
            "u0 0 0 100 1000 CELL\n");
  status = parseGpFile("tests/fixture_parser_bad.gp", &model);
  expectTrue(!status.ok, "missing CELL orientation should fail");
  expectTrue(status.message.find("line") != std::string::npos, "diagnostic has line number");
}

void testPlacementModelHelpers() {
  Tech tech;
  tech.dbuPerMicron = 100;
  tech.die = Rect{7, 11, 1007, 211};
  tech.siteWidth = 10;
  tech.siteHeight = 20;
  expectTrue(!overlaps(Rect{0, 0, 10, 10}, Rect{10, 0, 20, 10}),
             "edge-touch rectangles must not overlap");
  expectTrue(contains(Rect{0, 0, 10, 10}, Rect{1, 1, 9, 9}),
             "containment failed");
  expectEq(snapDownToSite(tech, 36), static_cast<Dbu>(27), "snap down");
  expectEq(snapUpToSite(tech, 36), static_cast<Dbu>(37), "snap up");
  expectTrue(isSiteAligned(tech, 37), "site alignment with nonzero origin");
  expectTrue(isRowAligned(tech, 31), "row alignment with nonzero origin");
  expectEq(rowCount(tech), 10, "row count");
}

void testRowIntervals() {
  PlacementModel model = smallModel();
  model.obstacles.push_back(Obstacle{"m0", Rect{1000, 0, 2000, 1000}, ObjectType::Macro, "R0"});
  model.obstacles.push_back(Obstacle{"b0", Rect{3000, 1000, 5000, 3000}, ObjectType::Blockage, ""});
  std::vector<Row> rows;
  Status status = buildRowIntervals(model, &rows);
  expectTrue(status.ok, status.message);
  expectEq(rows.size(), static_cast<std::size_t>(3), "row count");
  expectEq(rows[0].intervals.size(), static_cast<std::size_t>(2), "row 0 split");
  expectEq(rows[0].intervals[0].xMax, static_cast<Dbu>(1000), "left split xMax");
  expectEq(rows[1].intervals.size(), static_cast<std::size_t>(2), "row 1 split");
  expectEq(rows[2].intervals.size(), static_cast<std::size_t>(2), "row 2 split");
}

void testAbacusAndTetris() {
  PlacementModel model = smallModel();
  model.cells.push_back(makeCell("a", Point{0, 0}, 1000, 1000, "R0"));
  model.cells.push_back(makeCell("b", Point{0, 0}, 1000, 1000, "R0"));
  RowInterval interval{0, 0, 0, 0, 2000, 0, {}};
  IntervalSolveResult solved = solveIntervalAbacus(model, interval, {0, 1});
  expectTrue(solved.ok, solved.message);
  expectEq(solved.xByOrder[0], static_cast<Dbu>(0), "first abacus x");
  expectEq(solved.xByOrder[1], static_cast<Dbu>(1000), "second abacus x");
  solved = solveIntervalAbacus(model, RowInterval{0, 0, 0, 0, 1500, 0, {}}, {0, 1});
  expectTrue(!solved.ok, "over-capacity interval should fail");

  PlacementModel clusterModel = smallModel();
  clusterModel.cells.push_back(makeCell("c0", Point{0, 0}, 1000, 1000, "R0"));
  clusterModel.cells.push_back(makeCell("c1", Point{1900, 0}, 1000, 1000, "R0"));
  clusterModel.cells.push_back(makeCell("c2", Point{1900, 0}, 1000, 1000, "R0"));
  solved = solveIntervalAbacus(clusterModel, RowInterval{0, 0, 0, 0, 3500, 0, {}},
                               {0, 1, 2});
  expectTrue(solved.ok, solved.message);
  expectEq(solved.xByOrder[0], static_cast<Dbu>(0), "cluster first x");
  expectEq(solved.xByOrder[1], static_cast<Dbu>(1400), "cluster second x");
  expectEq(solved.xByOrder[2], static_cast<Dbu>(2400), "cluster third x");

  std::vector<Row> rows;
  Status status = buildRowIntervals(model, &rows);
  expectTrue(status.ok, status.message);
  model.cells[0].placed = Point{0, 0};
  model.cells[0].placedValid = true;
  rows[0].intervals[0].cellIds.push_back(0);
  recomputeOccupiedWidth(model, &rows[0].intervals[0]);
  status = tetrisPlaceCell(&model, &rows, 1);
  expectTrue(status.ok, status.message);
  expectTrue(model.cells[1].placedValid, "tetris placed cell");
  expectTrue(model.cells[1].placed.x >= 1000, "tetris avoided occupied span");
}

void testBaselineAndValidator() {
  PlacementModel model = smallModel();
  model.obstacles.push_back(Obstacle{"m0", Rect{1000, 0, 2000, 1000}, ObjectType::Macro, "R0"});
  model.cells.push_back(makeCell("a", Point{0, 0}, 1000, 1000, "R0"));
  model.cells.push_back(makeCell("b", Point{0, 0}, 1000, 1000, "R0"));
  std::vector<Row> rows;
  expectTrue(buildRowIntervals(model, &rows).ok, "row build failed");
  Status status = legalizePlacement(&model, &rows);
  expectTrue(status.ok, status.message);
  std::vector<std::string> diagnostics = validateLegality(model, rows);
  expectEq(diagnostics.size(), static_cast<std::size_t>(0), "valid legality diagnostics");

  model.cells[0].placed.x = 1050;
  diagnostics = validateLegality(model, rows);
  expectTrue(!diagnostics.empty(), "off-site/obstacle placement should fail");
}

void testBaselineSearchesPastEarlyFragmentedRows() {
  PlacementModel model;
  model.tech.dbuPerMicron = 1000;
  model.tech.die = Rect{0, 0, 100000, 30000};
  model.tech.siteWidth = 1000;
  model.tech.siteHeight = 1000;
  model.obstacles.push_back(
      Obstacle{"wall", Rect{0, 0, 90000, 12000}, ObjectType::Blockage, ""});
  model.cells.push_back(makeCell("near_farther_row", Point{0, 0}, 1000, 1000, "R0"));

  std::vector<Row> rows;
  expectTrue(buildRowIntervals(model, &rows).ok, "row build failed");
  Status status = legalizePlacement(&model, &rows);
  expectTrue(status.ok, status.message);
  expectEq(model.cells[0].placed.x, static_cast<Dbu>(0), "farther row x");
  expectEq(model.cells[0].placed.y, static_cast<Dbu>(12000), "farther row y");
}

void testMetricsAndWriter() {
  PlacementModel model;
  model.tech.dbuPerMicron = 1;
  model.tech.die = Rect{0, 0, 20, 10};
  model.tech.siteWidth = 1;
  model.tech.siteHeight = 10;
  model.cells.push_back(makeCell("a[0]", Point{0, 0}, 10, 10, "MX", true, Point{0, 0}));
  Metrics metrics = evaluateMetrics(model, 0.5, 100.0);
  expectEq(metrics.totalGrids, 2, "all non-macro grid count");
  expectEq(metrics.overflowGrids, 0, "threshold equality is not overflow");
  metrics = evaluateMetrics(model, 0.0, 45.0);
  expectEq(metrics.overflowGrids, 1, "one overflow grid");
  expectEq(metrics.dorPercent, 50.0, "one of two non-macro grids overflows");
  expectEq(metrics.quality, metrics.dorPercent, "alpha zero quality");

  model.obstacles.push_back(Obstacle{"m0", Rect{10, 0, 20, 10}, ObjectType::Macro, "R0"});
  metrics = evaluateMetrics(model, 0.0, 45.0);
  expectEq(metrics.totalGrids, 1, "macro grid excluded from denominator");

  Status status = writeTcl(model, "tests/out_writer.tcl");
  expectTrue(status.ok, status.message);
  std::string text = readFile("tests/out_writer.tcl");
  expectTrue(text.find("place_cell -inst_name {a[0]} -orient MX -origin {0.000000 10.000000}") !=
                 std::string::npos,
             "writer converts lower-left to mirrored OpenROAD origin");
  expectTrue(text.find("detailed_placement") == std::string::npos,
             "writer must not emit detailed_placement");

  PlacementModel safeNameModel = model;
  safeNameModel.obstacles.clear();
  safeNameModel.cells.clear();
  safeNameModel.cells.push_back(
      makeCell("FE_OFC15837_n_65636", Point{0, 0}, 10, 10, "R0", true, Point{0, 0}));
  status = writeTcl(safeNameModel, "tests/out_writer_safe_name.tcl");
  expectTrue(status.ok, status.message);
  text = readFile("tests/out_writer_safe_name.tcl");
  expectTrue(text == "place_cell -inst_name FE_OFC15837_n_65636 -orient R0 -origin {0.000000 0.000000}\n",
             "safe instance names and orientations must be emitted without braces");
}

void testDisplacementRepairReinsertsCloserCell() {
  PlacementModel model = smallModel();
  model.cells.push_back(
      makeCell("wandered", Point{0, 0}, 1000, 1000, "R0", true, Point{0, 1000}));

  std::vector<Row> rows;
  expectTrue(buildRowIntervals(model, &rows).ok, "row build failed");
  rows[1].intervals[0].cellIds.push_back(0);
  recomputeOccupiedWidth(model, &rows[1].intervals[0]);

  Status status = runDisplacementRepair(&model, &rows);
  expectTrue(status.ok, status.message);
  expectEq(model.cells[0].placed.x, static_cast<Dbu>(0), "reinserted x");
  expectEq(model.cells[0].placed.y, static_cast<Dbu>(0), "reinserted y");
}

void testDorRepairMovesOverflowContributor() {
  PlacementModel model;
  model.tech.dbuPerMicron = 1000;
  model.tech.die = Rect{0, 0, 21000, 1000};
  model.tech.siteWidth = 1000;
  model.tech.siteHeight = 1000;
  model.obstacles.push_back(
      Obstacle{"b0", Rect{10000, 0, 11000, 1000}, ObjectType::Blockage, ""});
  model.cells.push_back(makeCell("a", Point{0, 0}, 5000, 1000, "R0"));
  model.cells.push_back(makeCell("b", Point{0, 0}, 5000, 1000, "R0"));

  std::vector<Row> rows;
  expectTrue(buildRowIntervals(model, &rows).ok, "row build failed");
  expectTrue(legalizePlacement(&model, &rows).ok, "baseline legalization failed");
  Metrics before = evaluateMetrics(model, 0.0, 75.0);
  expectEq(before.overflowGrids, 1, "baseline overflow count");

  Status status = runDorRepair(&model, &rows, 0.0, 75.0);
  expectTrue(status.ok, status.message);
  std::vector<std::string> diagnostics = validateLegality(model, rows);
  expectEq(diagnostics.size(), static_cast<std::size_t>(0), "repair legality");
  Metrics after = evaluateMetrics(model, 0.0, 75.0);
  expectEq(after.overflowGrids, 0, "repaired overflow count");
}

void testUnsupportedCells() {
  PlacementModel model = smallModel();
  model.cells.push_back(makeCell("tall", Point{0, 0}, 100, 2000, "R0"));
  Status status = validateSupportedCells(model);
  expectTrue(!status.ok, "multi-row cell should be rejected");
  expectTrue(status.message.find("tall") != std::string::npos, "diagnostic names cell");
}

}  // namespace

int main() {
  run("Parser", testParserValidAndInvalid);
  run("PlacementModel", testPlacementModelHelpers);
  run("RowIntervals", testRowIntervals);
  run("AbacusAndTetris", testAbacusAndTetris);
  run("BaselineAndValidator", testBaselineAndValidator);
  run("BaselineSearch", testBaselineSearchesPastEarlyFragmentedRows);
  run("MetricsAndWriter", testMetricsAndWriter);
  run("DisplacementRepair", testDisplacementRepairReinsertsCloserCell);
  run("DorRepair", testDorRepairMovesOverflowContributor);
  run("UnsupportedCells", testUnsupportedCells);

  if (failures != 0) {
    std::cerr << failures << " test failure(s)\n";
    return 1;
  }
  std::cerr << "All tests passed\n";
  return 0;
}
