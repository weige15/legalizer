#pragma once

#include <string>
#include <vector>

#include "density_estimator.hpp"
#include "placement_model.hpp"
#include "row_interval_builder.hpp"

namespace legalizer {

class Legalizer {
 public:
  Legalizer(Design& design, const std::vector<LegalRow>& rows,
            DensityEstimator& density, double alpha);

  bool legalize(std::string& error);

 private:
  struct Segment {
    int64_t x_min = 0;
    int64_t x_max = 0;
  };

  struct Candidate {
    size_t row_index = 0;
    int64_t x = 0;
    int64_t y = 0;
    double total_cost = 0.0;
    double displacement_cost = 0.0;
    double density_cost = 0.0;
  };

  std::vector<size_t> placementOrder() const;
  std::vector<size_t> rowOrderForCell(const Cell& cell) const;
  int64_t constrainedStartCount(const Cell& cell) const;
  std::vector<RowInterval> commonFreeIntervals(size_t start_row,
                                               int64_t row_span) const;
  std::vector<RowInterval> subtractOccupancy(
      std::vector<RowInterval> intervals, size_t start_row,
      int64_t row_span) const;
  bool evaluateCandidatesForRow(size_t cell_index, size_t start_row,
                                Candidate& best, bool& has_best,
                                bool displacement_only = false) const;
  bool isBetterCandidate(const Candidate& cand, const Candidate& best) const;
  void commit(size_t cell_index, const Candidate& candidate);
  void removeFromOccupancy(size_t cell_index);
  bool findBestCandidate(size_t cell_index, size_t row_budget,
                         bool displacement_only, Candidate& best) const;
  void repairDisplacementTail();
  double displacementUm(const Cell& cell) const;
  int64_t rowIndexForY(int64_t y) const;
  bool segmentOverlaps(const std::vector<Segment>& segments, int64_t x_min,
                       int64_t x_max) const;

  Design& design_;
  const std::vector<LegalRow>& rows_;
  DensityEstimator& density_;
  double alpha_ = 0.0;
  std::vector<std::vector<Segment>> occupancy_;
};

}  // namespace legalizer
