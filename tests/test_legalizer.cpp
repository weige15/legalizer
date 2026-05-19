#include "density_estimator.h"
#include "gp_parser.h"
#include "legalizer.h"
#include "row_interval_builder.h"
#include "tcl_writer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace legalizer;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Design basicDesign() {
    Design design;
    design.dbu_per_micron = 100;
    design.die = Rect{0, 0, 1000, 300};
    design.site_width = 100;
    design.site_height = 100;
    return design;
}

void writeText(const std::string& path, const std::string& text) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write test fixture: " + path);
    }
    out << text;
}

std::string readText(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void testGeometry() {
    Rect a{0, 0, 10, 10};
    Rect b{10, 0, 20, 10};
    Rect c{9, 9, 20, 20};
    require(!overlaps(a, b), "touching half-open rectangles must not overlap");
    require(overlaps(a, c), "intersecting rectangles must overlap");
    require(area(a) == 100, "rectangle area is integer DBU area");
    require(alignUp(11, 0, 10) == 20, "alignUp failed");
    require(alignDown(19, 0, 10) == 10, "alignDown failed");
}

void testParser() {
    Design design = parseGpFile("tests/fixture_one_cell.gp");
    require(design.cells.size() == 1, "parser did not read one movable cell");
    require(design.cells[0].name == "u0", "parser did not preserve cell name");
    require(design.cells[0].input_index == 0, "parser did not preserve input order");

    const std::string mixed_path = "tests/tmp_mixed.gp";
    writeText(mixed_path,
              "DBU_Per_Micron 100\n"
              "DieArea_LL 0 0\n"
              "DieArea_UR 1000 1000\n"
              "Site_Width 100\n"
              "Site_Height 100\n"
              "Name LLX LLY Width Height Type\n"
              "cellA 0 0 100 100 CELL\n"
              "macroA 100 0 100 100 MACRO\n"
              "blkA 300 0 100 100 BLOCKAGE\n");
    Design mixed = parseGpFile(mixed_path);
    require(mixed.cells.size() == 1, "parser mixed fixture cell count wrong");
    require(mixed.obstacles.size() == 2, "parser mixed fixture obstacle count wrong");

    const std::string bad_path = "tests/tmp_bad.gp";
    writeText(bad_path,
              "DBU_Per_Micron 100\n"
              "DieArea_LL 0 0\n"
              "DieArea_UR 1000 1000\n"
              "Site_Width 100\n"
              "Site_Height 100\n"
              "\n"
              "Name LLX LLY Width Height Type\n"
              "x 0 0 100 100 FOO\n");
    bool failed = false;
    try {
        (void)parseGpFile(bad_path);
    } catch (const std::exception& ex) {
        failed = std::string(ex.what()).find("line ") != std::string::npos;
    }
    require(failed, "parser should reject unknown types with a line-numbered diagnostic");
}

void testRows() {
    Design design = basicDesign();
    design.obstacles.push_back(Obstacle{"m0", Rect{200, 0, 400, 100}, InstanceType::Macro});
    design.obstacles.push_back(Obstacle{"b0", Rect{450, 100, 650, 200}, InstanceType::Blockage});
    std::vector<Row> rows = buildRowSegments(design);
    require(rows.size() == 3, "row builder row count wrong");
    require(rows[0].segments.size() == 2, "macro should split first row");
    require(rows[0].segments[0].lx == 0 && rows[0].segments[0].ux == 200,
            "first free segment wrong");
    require(rows[0].segments[1].lx == 400 && rows[0].segments[1].ux == 1000,
            "second free segment wrong");
    require(rows[1].segments.size() == 2, "blockage should split second row");
    require(rows[2].segments.size() == 1, "non-overlapping obstacle touched wrong row");
}

void testAbacus() {
    std::vector<RowCell> cells{
        RowCell{0, 9, 10, 0},
        RowCell{1, 10, 10, 1},
    };
    RowPlacementResult result = placeRowAbacus(cells, Interval{0, 30}, 0, 5);
    require(result.feasible, "abacus cluster merge should be feasible");
    require(result.origins[0] + 10 <= result.origins[1], "abacus output overlaps");
    require(isSiteAligned(result.origins[0], 0, 5), "abacus output not site-aligned");

    RowPlacementResult left = placeRowAbacus({RowCell{0, -100, 10, 0}}, Interval{0, 30}, 0, 5);
    require(left.feasible && left.origins[0] == 0, "abacus left clamp failed");
    RowPlacementResult right = placeRowAbacus({RowCell{0, 100, 10, 0}}, Interval{0, 30}, 0, 5);
    require(right.feasible && right.origins[0] == 20, "abacus right clamp failed");

    RowPlacementResult infeasible = placeRowAbacus({RowCell{0, 0, 20, 0}, RowCell{1, 0, 20, 1}},
                                                   Interval{0, 30}, 0, 5);
    require(!infeasible.feasible, "abacus should reject too-full rows");

    RowPlacementResult tie = placeRowAbacus({RowCell{2, 0, 10, 2}, RowCell{1, 0, 10, 1}},
                                            Interval{0, 30}, 0, 5);
    require(tie.feasible && tie.origins[1] < tie.origins[2], "abacus tie break by input index failed");
}

void testDensity() {
    Design design;
    design.dbu_per_micron = 100;
    design.die = Rect{0, 0, 2000, 2000};
    design.site_width = 100;
    design.site_height = 100;
    DensityEstimator density(design);
    require(density.columns() == 2 && density.rows() == 2, "density grid dimensions wrong");
    density.addRect(Rect{0, 0, 1000, 1000});
    require(std::fabs(density.dor(45.0) - 25.0) < 1e-9, "density DOR for one full grid wrong");

    design.obstacles.push_back(Obstacle{"h0", Rect{1000, 1000, 2000, 2000}, InstanceType::Macro});
    DensityEstimator macro_density(design);
    require(macro_density.countedGridCount() == 3, "fully macro-covered grid should be excluded");
}

void testLegalizerAndChecker() {
    Design design = basicDesign();
    design.obstacles.push_back(Obstacle{"m0", Rect{200, 0, 400, 100}, InstanceType::Macro});
    design.cells.push_back(MovableCell{"a", 200, 0, 100, 100, 0, {}});
    design.cells.push_back(MovableCell{"b", 300, 0, 100, 100, 1, {}});
    std::vector<Row> rows = buildRowSegments(design);
    legalizeDesign(design, rows, LegalizerConfig{0.7, 45.0});
    std::string error;
    require(checkLegality(design, rows, error), "legalizer produced illegal macro-gap placement: " + error);
    require(!overlaps(placedRect(design.cells[0]), design.obstacles[0].rect), "cell a overlaps macro");
    require(!overlaps(placedRect(design.cells[1]), design.obstacles[0].rect), "cell b overlaps macro");

    Design tall = basicDesign();
    tall.cells.push_back(MovableCell{"tall", 0, 0, 100, 200, 0, {}});
    tall.cells.push_back(MovableCell{"one", 0, 0, 100, 100, 1, {}});
    std::vector<Row> tall_rows = buildRowSegments(tall);
    legalizeDesign(tall, tall_rows, LegalizerConfig{0.7, 45.0});
    require(checkLegality(tall, tall_rows, error), "tall-cell placement failed legality: " + error);
}

void testLegalityViolations() {
    Design design = basicDesign();
    design.cells.push_back(MovableCell{"a", 0, 0, 100, 100, 0, Placement{10, 0}});
    std::vector<Row> rows = buildRowSegments(design);
    std::string error;
    require(!checkLegality(design, rows, error), "off-site X should fail legality");

    design.cells[0].legal = Placement{0, 50};
    require(!checkLegality(design, rows, error), "off-row Y should fail legality");

    design.cells[0].legal = Placement{0, 0};
    design.cells.push_back(MovableCell{"b", 0, 0, 100, 100, 1, Placement{0, 0}});
    require(!checkLegality(design, rows, error), "movable overlap should fail legality");
}

void testWriter() {
    Design design = basicDesign();
    design.cells.push_back(MovableCell{"a", 0, 0, 100, 100, 0, Placement{150, 0}});
    design.cells.push_back(MovableCell{"b", 0, 0, 100, 100, 1, Placement{300, 100}});
    const std::string path = "tests/tmp_writer.tcl";
    writeTclFile(design, path);
    std::string text = readText(path);
    require(text.find("detailed_placement") == std::string::npos, "writer emitted detailed_placement");
    require(text.find("place_cell -inst_name a -orient R0 -origin {1.5 0}") != std::string::npos,
            "writer did not convert DBU to microns");
    require(text.find("a") < text.find("b"), "writer did not preserve input order");
}

void testEndToEndFixture() {
    Design design = parseGpFile("tests/fixture_one_cell.gp");
    std::vector<Row> rows = buildRowSegments(design);
    legalizeDesign(design, rows, LegalizerConfig{0.7, 45.0});
    std::string error;
    require(checkLegality(design, rows, error), "one-cell end-to-end legality failed: " + error);
}

void testCli() {
    int missing = std::system("./Legalizer > tests/tmp_cli_missing.log 2>&1");
    require(missing != 0, "CLI should reject missing arguments");

    int bad_alpha = std::system("./Legalizer not-a-number 45 tests/fixture_one_cell.gp tests/tmp_cli_bad.tcl > tests/tmp_cli_bad.log 2>&1");
    require(bad_alpha != 0, "CLI should reject non-numeric alpha");

    int valid = std::system("./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/tmp_cli_valid.tcl > tests/tmp_cli_valid.log 2>&1");
    require(valid == 0, "CLI should accept valid assignment arguments");
    std::string text = readText("tests/tmp_cli_valid.tcl");
    require(text.find("place_cell -inst_name u0 -orient R0") != std::string::npos,
            "CLI output does not contain expected placement command");
}

} // namespace

int main() {
    const std::vector<std::pair<std::string, void (*)()>> tests = {
        {"geometry", testGeometry},
        {"parser", testParser},
        {"rows", testRows},
        {"abacus", testAbacus},
        {"density", testDensity},
        {"legalizer", testLegalizerAndChecker},
        {"legality violations", testLegalityViolations},
        {"writer", testWriter},
        {"end-to-end fixture", testEndToEndFixture},
        {"cli", testCli},
    };

    try {
        for (const auto& test : tests) {
            test.second();
            std::cout << "[PASS] " << test.first << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
