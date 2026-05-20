#include "density_estimator.h"
#include "gp_parser.h"
#include "legalizer.h"
#include "row_interval_builder.h"
#include "tcl_writer.h"

#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

using namespace legalizer;

namespace {

void testGeometry() {
    assert(overlaps(Rect{0, 0, 10, 10}, Rect{9, 0, 20, 10}));
    assert(!overlaps(Rect{0, 0, 10, 10}, Rect{10, 0, 20, 10}));
    assert(overlapArea(Rect{0, 0, 10, 10}, Rect{5, 5, 20, 20}) == 25);
    assert(snapUpToGrid(101, 0, 100) == 200);
    assert(snapDownToGrid(199, 0, 100) == 100);
}

void testParserAndIntervals() {
    PlacementModel one = parseGpFile("tests/fixture_one_cell.gp");
    assert(one.dbu_per_micron == 1000);
    assert(one.cell_ids.size() == 1);
    assert(one.obstacle_ids.empty());
    auto intervals = buildRowIntervals(one);
    assert(intervals.size() == 2);

    PlacementModel split = parseGpFile("tests/fixture_macro_split.gp");
    auto split_intervals = buildRowIntervals(split);
    assert(split_intervals.size() == 2);
    assert(split_intervals[0].x1 <= 2000);
    assert(split_intervals[1].x0 >= 3000);

    bool failed = false;
    try {
        (void)parseGpFile("tests/fixture_malformed.gp");
    } catch (const PlacementError&) {
        failed = true;
    }
    assert(failed);
}

void testRowSolver() {
    PlacementModel model = parseGpFile("tests/fixture_two_overlap.gp");
    auto intervals = buildRowIntervals(model);
    std::vector<Point> placed = solveRowInterval(model, intervals[0], model.cell_ids);
    ValidationResult valid = validatePlacement(model, placed, intervals, 0.7, 45.0);
    assert(valid.ok);
    assert(placed[model.cell_ids[0]].x == 0);
    assert(placed[model.cell_ids[1]].x >= 1000);
}

void testEndToEndFixtures() {
    for (const std::string path : {"tests/fixture_one_cell.gp", "tests/fixture_two_overlap.gp",
                                   "tests/fixture_macro_split.gp", "tests/fixture_blockage.gp"}) {
        PlacementModel model = parseGpFile(path);
        auto intervals = buildRowIntervals(model);
        LegalizationResult result = legalize(model, intervals, 0.4, 45.0);
        ValidationResult valid = validatePlacement(model, result.placements, intervals, 0.4, 45.0);
        assert(valid.ok);
    }
}

void testWriter() {
    PlacementModel model = parseGpFile("tests/fixture_one_cell.gp");
    auto intervals = buildRowIntervals(model);
    LegalizationResult result = legalize(model, intervals, 0.7, 45.0);
    writeTcl(model, result.placements, "tests/out_writer.tcl");
    std::ifstream in("tests/out_writer.tcl");
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();
    assert(text.find("place_cell -inst_name u0 -orient R0 -origin {") != std::string::npos);
    assert(text.find("detailed_placement") == std::string::npos);
}

}  // namespace

int main() {
    testGeometry();
    testParserAndIntervals();
    testRowSolver();
    testEndToEndFixtures();
    testWriter();
    return 0;
}
