#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "placement_model.hpp"

namespace legalizer {

class DensityEstimator {
 public:
  DensityEstimator(const Design& design, double threshold);

  double scoreCandidate(const Rect& rect) const;
  double overflowProxy() const;
  double overflowProxyWithCandidate(const Rect& rect) const;
  void commit(const Rect& rect);
  void rebuildMovableOccupancy();

  int64_t gridSize() const { return grid_size_; }

 private:
  struct Grid {
    int64_t macro_area = 0;
    int64_t movable_area = 0;
  };

  uint64_t key(int64_t gx, int64_t gy) const;
  Rect gridRect(int64_t gx, int64_t gy) const;
  int64_t gridArea(int64_t gx, int64_t gy) const;
  void addArea(const Rect& rect, bool macro);

  const Design& design_;
  double threshold_ = 0.0;
  int64_t grid_size_ = 1;
  int64_t cols_ = 0;
  int64_t rows_ = 0;
  std::unordered_map<uint64_t, Grid> grids_;
};

}  // namespace legalizer
