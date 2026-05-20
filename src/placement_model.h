#ifndef PLACEMENT_MODEL_H
#define PLACEMENT_MODEL_H

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace legalizer {

struct Point {
    int64_t x = 0;
    int64_t y = 0;
};

struct Rect {
    int64_t x0 = 0;
    int64_t y0 = 0;
    int64_t x1 = 0;
    int64_t y1 = 0;
};

enum class InstanceType {
    Cell,
    Macro,
    Blockage
};

struct Instance {
    std::string name;
    Rect original;
    InstanceType type = InstanceType::Cell;
    size_t input_order = 0;
};

struct RowInterval {
    int row_index = 0;
    int64_t y = 0;
    int64_t x0 = 0;
    int64_t x1 = 0;
};

struct Metrics {
    double avg_displacement_um = 0.0;
    double normalized_displacement = 0.0;
    double dor_percent = 0.0;
    double flow_quality = 0.0;
    double handout_quality = 0.0;
};

struct ValidationResult {
    bool ok = false;
    std::vector<std::string> errors;
    Metrics metrics;
};

class PlacementError : public std::runtime_error {
public:
    explicit PlacementError(const std::string& message) : std::runtime_error(message) {}
};

class PlacementModel {
public:
    int64_t dbu_per_micron = 0;
    Rect die;
    int64_t site_width = 0;
    int64_t site_height = 0;
    std::vector<Instance> instances;
    std::vector<size_t> cell_ids;
    std::vector<size_t> obstacle_ids;

    void validateBasic() const;
    void rebuildIndexes();

    int64_t width(const Instance& inst) const;
    int64_t height(const Instance& inst) const;
    int64_t dieWidth() const;
    int64_t dieHeight() const;
    int rowCount() const;
    int rowIndexForY(int64_t y) const;
    int64_t rowY(int row_index) const;
    bool isRowAligned(int64_t y) const;
    bool isSiteAlignedX(int64_t x) const;
    bool isSingleRowCell(size_t id) const;
    Rect rectAt(size_t id, Point ll) const;
    double dbuToMicron(int64_t value) const;
};

std::string typeToString(InstanceType type);
InstanceType parseInstanceType(const std::string& text);
bool overlaps(const Rect& a, const Rect& b);
int64_t overlapArea(const Rect& a, const Rect& b);
bool contains(const Rect& outer, const Rect& inner);
int64_t snapUpToGrid(int64_t x, int64_t origin, int64_t step);
int64_t snapDownToGrid(int64_t x, int64_t origin, int64_t step);
int64_t manhattan(const Point& a, const Point& b);

}  // namespace legalizer

#endif
