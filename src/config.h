#pragma once

#include <string>

struct Config {
  double alpha = 0.0;
  double threshold = 0.0;
  std::string inputPath;
  std::string outputPath;
};

Config parseConfig(int argc, char **argv);

