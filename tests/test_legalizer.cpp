#include "src/config.h"
#include "src/density_grid.h"
#include "src/gp_parser.h"
#include "src/legalizer.h"
#include "src/row_model.h"
#include "src/tcl_writer.h"
#include "src/validation.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
std::string tempPath(const std::string &name) { return "/tmp/legalizer_" + name; }

void writeFile(const std::string &path, const std::string &text) {
  std::ofstream out(path);
  out << text;
}

Design tinyDesign() {
  Design d;
  d.dbuPerMicron = 1000;
  d.die = Rect{0, 0, 10000, 4000};
  d.siteWidth = 1000;
  d.siteHeight = 1000;
  d.cells.push_back(Cell{"a", Rect{0, 0, 1000, 1000}, 0});
  d.cells.push_back(Cell{"b", Rect{0, 0, 1000, 1000}, 1});
  return d;
}

void testConfig() {
  const char *argv[] = {"Legalizer", "0.7", "45", "in.gp", "out.tcl"};
  Config c = parseConfig(5, const_cast<char **>(argv));
  assert(c.alpha == 0.7);
  bool threw = false;
  const char *bad[] = {"Legalizer", "0.7x", "45", "in.gp", "out.tcl"};
  try {
    (void)parseConfig(5, const_cast<char **>(bad));
  } catch (const std::exception &) {
    threw = true;
  }
  assert(threw);

  threw = false;
  const char *badAlpha[] = {"Legalizer", "1.1", "45", "in.gp", "out.tcl"};
  try {
    (void)parseConfig(5, const_cast<char **>(badAlpha));
  } catch (const std::exception &) {
    threw = true;
  }
  assert(threw);

  threw = false;
  const char *badThreshold[] = {"Legalizer", "0.7", "-1", "in.gp", "out.tcl"};
  try {
    (void)parseConfig(5, const_cast<char **>(badThreshold));
  } catch (const std::exception &) {
    threw = true;
  }
  assert(threw);
}

void testParser() {
  const std::string path = tempPath("parser.gp");
  writeFile(path,
            "DBU_Per_Micron 1000\n"
            "DieArea_LL 0 0\n"
            "DieArea_UR 10000 2000\n"
            "Site_Width 1000\n"
            "Site_Height 1000\n"
            "\n"
            "Name LLX LLY Width Height Type\n"
            "u1 0 0 1000 1000 CELL\n"
            "m1 2000 0 1000 2000 MACRO\n"
            "b0 4000 0 1000 1000 BLOCKAGE\n");
  Design d = parseGpFile(path);
  assert(d.cells.size() == 1);
  assert(d.obstacles.size() == 2);
  assert(d.cells[0].name == "u1");

  const std::string bad = tempPath("bad.gp");
  writeFile(bad,
            "DBU_Per_Micron 1000\nDieArea_LL 0 0\nDieArea_UR 1000 1000\nSite_Width 100\n"
            "Site_Height 100\nName LLX LLY Width Height Type\nx 0 0 1 1 THING\n");
  bool threw = false;
  try {
    (void)parseGpFile(bad);
  } catch (const std::exception &) {
    threw = true;
  }
  assert(threw);

  const std::string overflow = tempPath("overflow.gp");
  writeFile(overflow,
            "DBU_Per_Micron 1000\nDieArea_LL 0 0\nDieArea_UR 1000 1000\nSite_Width 100\n"
            "Site_Height 100\nName LLX LLY Width Height Type\nx " +
                std::to_string(std::numeric_limits<Coord>::max()) + " 0 1 1 CELL\n");
  threw = false;
  try {
    (void)parseGpFile(overflow);
  } catch (const std::exception &) {
    threw = true;
  }
  assert(threw);
}

void testGeometry() {
  assert(!overlaps(Rect{0, 0, 10, 10}, Rect{10, 0, 20, 10}));
  assert(overlaps(Rect{0, 0, 10, 10}, Rect{9, 0, 20, 10}));
  assert(snapUp(11, 0, 10) == 20);
  assert(snapDown(19, 0, 10) == 10);
}

void testRowsAndValidation() {
  Design d = tinyDesign();
  d.obstacles.push_back(Obstacle{"m", Rect{2000, 0, 4000, 2000}, ObjectType::Macro, 2});
  RowModel rows(d);
  assert(rows.canPlace(d.cells[0], 0, 0));
  assert(!rows.canPlace(d.cells[0], 2000, 0));
  assert(rows.commit(d.cells[0], 0, 0));
  assert(!rows.canPlace(d.cells[1], 0, 0));
  assert(rows.canPlace(d.cells[1], 1000, 0));
  assert(rows.uncommit(d.cells[0], 0, 0));
}

void testValidationFailures() {
  Design d = tinyDesign();
  RowModel rows(d);
  std::string error;
  assert(!validateDesign(d, rows, &error));

  d.cells[0].placed = true;
  d.cells[0].x = 500;
  d.cells[0].y = 0;
  d.cells[1].placed = true;
  d.cells[1].x = 1000;
  d.cells[1].y = 0;
  assert(!validateDesign(d, rows, &error));

  d.cells[0].x = 0;
  d.cells[1].x = 0;
  assert(!validateDesign(d, rows, &error));
}

void testDensity() {
  Design d = tinyDesign();
  DensityGrid grid(d, 1.0);
  assert(grid.estimateDOR() == 0.0);
  grid.addCell(d.cells[0], 0, 0);
  assert(grid.estimateDOR() > 0.0);
  grid.removeCell(d.cells[0], 0, 0);
  assert(grid.estimateDOR() == 0.0);
}

void testLegalizeAndWriter() {
  Design d = tinyDesign();
  RowModel rows(d);
  DensityGrid grid(d, 45.0);
  legalizeDesign(d, rows, grid, 0.7);
  refineDesign(d, rows, grid, 0.7);
  std::string error;
  assert(validateDesign(d, rows, &error));
  assert(d.cells[0].placed && d.cells[1].placed);
  assert(!(d.cells[0].x == d.cells[1].x && d.cells[0].y == d.cells[1].y));

  const std::string out = tempPath("out.tcl");
  writeTcl(d, out);
  std::ifstream in(out);
  std::stringstream buffer;
  buffer << in.rdbuf();
  const std::string text = buffer.str();
  assert(text.find("place_cell -inst_name a -orient R0 -origin") != std::string::npos);
  assert(text.find("detailed_placement") == std::string::npos);
}

void testEndToEndFile() {
  const std::string gp = tempPath("e2e.gp");
  writeFile(gp,
            "DBU_Per_Micron 1000\n"
            "DieArea_LL 0 0\n"
            "DieArea_UR 6000 2000\n"
            "Site_Width 1000\n"
            "Site_Height 1000\n\n"
            "Name LLX LLY Width Height Type\n"
            "a 0 0 1000 1000 CELL\n"
            "b 0 0 1000 1000 CELL\n"
            "m 3000 0 1000 2000 MACRO\n");
  Design d = parseGpFile(gp);
  RowModel rows(d);
  DensityGrid grid(d, 45.0);
  legalizeDesign(d, rows, grid, 1.0);
  std::string error;
  assert(validateDesign(d, rows, &error));
}
} // namespace

int main() {
  testConfig();
  testParser();
  testGeometry();
  testRowsAndValidation();
  testValidationFailures();
  testDensity();
  testLegalizeAndWriter();
  testEndToEndFile();
  return 0;
}
