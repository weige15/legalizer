#include "config.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {
double parseFiniteDouble(const char *text, const char *name) {
  errno = 0;
  char *end = nullptr;
  const double value = std::strtod(text, &end);
  if (text == end || *end != '\0' || errno == ERANGE || !std::isfinite(value)) {
    throw std::runtime_error(std::string("invalid ") + name + ": " + text);
  }
  return value;
}
} // namespace

Config parseConfig(int argc, char **argv) {
  if (argc != 5) {
    throw std::runtime_error("usage: ./Legalizer <alpha> <threshold> <input>.gp <output>.tcl");
  }
  Config cfg;
  cfg.alpha = parseFiniteDouble(argv[1], "alpha");
  cfg.threshold = parseFiniteDouble(argv[2], "threshold");
  cfg.inputPath = argv[3];
  cfg.outputPath = argv[4];
  return cfg;
}

