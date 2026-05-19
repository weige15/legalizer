#include "density_estimator.h"

#include <algorithm>
#include <stdexcept>

namespace legalizer {

DensityEstimator::DensityEstimator(const Design& design)
    : die_(design.die), grid_size_(10 * design.dbu_per_micron) {
    validateDesignMetadata(design);
    if (grid_size_ <= 0) {
        throw std::runtime_error("density grid size must be positive");
    }
    cols_ = static_cast<std::size_t>(ceilDiv(width(die_), grid_size_));
    rows_ = static_cast<std::size_t>(ceilDiv(height(die_), grid_size_));
    if (cols_ == 0) {
        cols_ = 1;
    }
    if (rows_ == 0) {
        rows_ = 1;
    }
    occupied_.assign(cols_ * rows_, 0);
    denom_area_.assign(cols_ * rows_, 0);
    excluded_.assign(cols_ * rows_, false);

    for (std::size_t y = 0; y < rows_; ++y) {
        for (std::size_t x = 0; x < cols_; ++x) {
            Rect g = gridRect(x, y);
            denom_area_[index(x, y)] = area(intersection(g, die_));
        }
    }

    for (std::size_t y = 0; y < rows_; ++y) {
        for (std::size_t x = 0; x < cols_; ++x) {
            Rect g = intersection(gridRect(x, y), die_);
            if (!isValid(g)) {
                excluded_[index(x, y)] = true;
                continue;
            }
            for (const Obstacle& obstacle : design.obstacles) {
                if (obstacle.type != InstanceType::Macro) {
                    continue;
                }
                Rect clipped = intersection(obstacle.rect, g);
                if (area(clipped) == area(g)) {
                    excluded_[index(x, y)] = true;
                    break;
                }
            }
        }
    }
}

std::size_t DensityEstimator::index(std::size_t x, std::size_t y) const {
    return y * cols_ + x;
}

Rect DensityEstimator::gridRect(std::size_t x, std::size_t y) const {
    Coord lx = die_.lx + static_cast<Coord>(x) * grid_size_;
    Coord ly = die_.ly + static_cast<Coord>(y) * grid_size_;
    return Rect{lx, ly, lx + grid_size_, ly + grid_size_};
}

void DensityEstimator::applyRect(const Rect& rect, Coord sign, std::vector<Coord>& occ) const {
    Rect clipped = intersection(rect, die_);
    if (!isValid(clipped)) {
        return;
    }
    Coord x0 = (clipped.lx - die_.lx) / grid_size_;
    Coord x1 = (clipped.ux - 1 - die_.lx) / grid_size_;
    Coord y0 = (clipped.ly - die_.ly) / grid_size_;
    Coord y1 = (clipped.uy - 1 - die_.ly) / grid_size_;
    x0 = std::max<Coord>(0, x0);
    y0 = std::max<Coord>(0, y0);
    x1 = std::min<Coord>(static_cast<Coord>(cols_) - 1, x1);
    y1 = std::min<Coord>(static_cast<Coord>(rows_) - 1, y1);
    for (Coord gy = y0; gy <= y1; ++gy) {
        for (Coord gx = x0; gx <= x1; ++gx) {
            Rect inter = intersection(clipped, gridRect(static_cast<std::size_t>(gx), static_cast<std::size_t>(gy)));
            occ[index(static_cast<std::size_t>(gx), static_cast<std::size_t>(gy))] += sign * area(inter);
        }
    }
}

void DensityEstimator::addRect(const Rect& rect) {
    applyRect(rect, 1, occupied_);
}

void DensityEstimator::removeRect(const Rect& rect) {
    applyRect(rect, -1, occupied_);
}

double DensityEstimator::dor(double threshold) const {
    return dorForOccupancy(occupied_, threshold);
}

double DensityEstimator::dorWithAddedRect(const Rect& rect, double threshold) const {
    std::vector<Coord> copy = occupied_;
    applyRect(rect, 1, copy);
    return dorForOccupancy(copy, threshold);
}

std::size_t DensityEstimator::countedGridCount() const {
    std::size_t count = 0;
    for (std::size_t i = 0; i < excluded_.size(); ++i) {
        if (!excluded_[i] && denom_area_[i] > 0) {
            ++count;
        }
    }
    return count;
}

double DensityEstimator::dorForOccupancy(const std::vector<Coord>& occ, double threshold) const {
    std::size_t total = 0;
    std::size_t overflow = 0;
    for (std::size_t i = 0; i < occ.size(); ++i) {
        if (excluded_[i] || denom_area_[i] <= 0) {
            continue;
        }
        ++total;
        double density = 100.0 * static_cast<double>(occ[i]) / static_cast<double>(denom_area_[i]);
        if (density > threshold) {
            ++overflow;
        }
    }
    if (total == 0) {
        return 0.0;
    }
    return 100.0 * static_cast<double>(overflow) / static_cast<double>(total);
}

} // namespace legalizer
