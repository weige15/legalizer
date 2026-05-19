#ifndef DENSITY_ESTIMATOR_H
#define DENSITY_ESTIMATOR_H

#include "placement_model.h"

#include <cstddef>
#include <vector>

namespace legalizer {

class DensityEstimator {
public:
    explicit DensityEstimator(const Design& design);

    void addRect(const Rect& rect);
    void removeRect(const Rect& rect);
    double dor(double threshold) const;
    double dorWithAddedRect(const Rect& rect, double threshold) const;
    Coord gridSize() const { return grid_size_; }
    std::size_t columns() const { return cols_; }
    std::size_t rows() const { return rows_; }
    std::size_t countedGridCount() const;

private:
    std::size_t index(std::size_t x, std::size_t y) const;
    Rect gridRect(std::size_t x, std::size_t y) const;
    void applyRect(const Rect& rect, Coord sign, std::vector<Coord>& occ) const;
    double dorForOccupancy(const std::vector<Coord>& occ, double threshold) const;

    Rect die_;
    Coord grid_size_ = 0;
    std::size_t cols_ = 0;
    std::size_t rows_ = 0;
    std::vector<Coord> occupied_;
    std::vector<Coord> denom_area_;
    std::vector<bool> excluded_;
};

} // namespace legalizer

#endif
