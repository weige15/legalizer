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

  struct RefinementMove {
    bool swaps_partner = false;
    size_t partner_index = 0;
    Rect cell_rect;
    Rect partner_rect;
    double max_displacement_cost = 0.0;
    double total_displacement_cost = 0.0;
    double density_cost = 0.0;
    double total_cost = 0.0;
  };

  std::vector<size_t> placementOrder() const;
  std::vector<size_t> rowOrderForCell(const Cell& cell) const;
  std::vector<RowInterval> commonFreeIntervals(size_t start_row,
                                               int64_t row_span) const;
  std::vector<RowInterval> subtractOccupancy(
      std::vector<RowInterval> intervals, size_t start_row,
      int64_t row_span) const;
  std::vector<int64_t> xCandidatesForInterval(const Cell& cell,
                                              const RowInterval& interval,
                                              int64_t width,
                                              int64_t radius_sites) const;
  bool findBestPlacement(size_t cell_index, size_t row_budget,
                         int64_t radius_sites, Candidate& best,
                         bool& has_best) const;
  bool evaluateCandidatesForRow(size_t cell_index, size_t start_row,
                                int64_t radius_sites, Candidate& best,
                                bool& has_best) const;
  bool isBetterCandidate(const Candidate& cand, const Candidate& best) const;
  bool isBetterRefinement(const RefinementMove& cand,
                          const RefinementMove& best) const;
  void commit(size_t cell_index, const Candidate& candidate);
  void addOccupancy(const Rect& rect, size_t start_row);
  void removeOccupancy(const Rect& rect, size_t start_row);
  bool rowIndexForY(int64_t y, size_t& row_index) const;
  bool placementIsLegalInCurrentOccupancy(const Cell& cell,
                                          const Rect& rect) const;
  std::vector<size_t> overlappingPlacedCells(const Rect& rect,
                                             size_t ignore_index,
                                             size_t limit) const;
  double displacementCost(const Cell& cell, const Rect& rect) const;
  bool refineOutliers();
  bool tryRefineOutlier(size_t cell_index, double outlier_threshold_um);
  bool evaluateEmptyRefinement(size_t cell_index, const Rect& old_rect,
                               const Rect& new_rect,
                               RefinementMove& best,
                               bool& has_best) const;
  bool evaluateSwapRefinement(size_t cell_index, size_t partner_index,
                              const Rect& old_rect, const Rect& new_rect,
                              double outlier_threshold_um,
                              RefinementMove& best, bool& has_best);
  bool segmentOverlaps(const std::vector<Segment>& segments, int64_t x_min,
                       int64_t x_max) const;

  Design& design_;
  const std::vector<LegalRow>& rows_;
  DensityEstimator& density_;
  double alpha_ = 0.0;
  std::vector<std::vector<Segment>> occupancy_;
};

}  // namespace legalizer
