#include "placement_model.h"

#include <algorithm>
#include <limits>

namespace legalizer {

namespace {

int64_t checkedSub(int64_t a, int64_t b, const char* what) {
    if ((b < 0 && a > std::numeric_limits<int64_t>::max() + b) ||
        (b > 0 && a < std::numeric_limits<int64_t>::min() + b)) {
        throw PlacementError(std::string("integer overflow while computing ") + what);
    }
    return a - b;
}

int64_t floorDiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    int64_t r = a % b;
    if (r != 0 && ((r > 0) != (b > 0))) {
        --q;
    }
    return q;
}

int64_t ceilDiv(int64_t a, int64_t b) {
    return -floorDiv(-a, b);
}

}  // namespace

void PlacementModel::validateBasic() const {
    if (dbu_per_micron <= 0) {
        throw PlacementError("DBU_Per_Micron must be positive");
    }
    if (site_width <= 0 || site_height <= 0) {
        throw PlacementError("site width and height must be positive");
    }
    if (die.x1 <= die.x0 || die.y1 <= die.y0) {
        throw PlacementError("die area must have positive width and height");
    }
    for (const auto& inst : instances) {
        if (inst.name.empty()) {
            throw PlacementError("instance name must not be empty");
        }
        if (inst.original.x1 <= inst.original.x0 || inst.original.y1 <= inst.original.y0) {
            throw PlacementError("instance " + inst.name + " has nonpositive dimensions");
        }
    }
}

void PlacementModel::rebuildIndexes() {
    cell_ids.clear();
    obstacle_ids.clear();
    for (size_t i = 0; i < instances.size(); ++i) {
        if (instances[i].type == InstanceType::Cell) {
            cell_ids.push_back(i);
        } else {
            obstacle_ids.push_back(i);
        }
    }
}

int64_t PlacementModel::width(const Instance& inst) const {
    return checkedSub(inst.original.x1, inst.original.x0, "instance width");
}

int64_t PlacementModel::height(const Instance& inst) const {
    return checkedSub(inst.original.y1, inst.original.y0, "instance height");
}

int64_t PlacementModel::dieWidth() const {
    return die.x1 - die.x0;
}

int64_t PlacementModel::dieHeight() const {
    return die.y1 - die.y0;
}

int PlacementModel::rowCount() const {
    return static_cast<int>(dieHeight() / site_height);
}

int PlacementModel::rowIndexForY(int64_t y) const {
    if (!isRowAligned(y)) {
        return -1;
    }
    int64_t idx = (y - die.y0) / site_height;
    if (idx < 0 || idx >= rowCount()) {
        return -1;
    }
    return static_cast<int>(idx);
}

int64_t PlacementModel::rowY(int row_index) const {
    return die.y0 + static_cast<int64_t>(row_index) * site_height;
}

bool PlacementModel::isRowAligned(int64_t y) const {
    return y >= die.y0 && y + site_height <= die.y1 && (y - die.y0) % site_height == 0;
}

bool PlacementModel::isSiteAlignedX(int64_t x) const {
    return x >= die.x0 && (x - die.x0) % site_width == 0;
}

bool PlacementModel::isSingleRowCell(size_t id) const {
    return height(instances.at(id)) == site_height;
}

Rect PlacementModel::rectAt(size_t id, Point ll) const {
    const auto& inst = instances.at(id);
    return Rect{ll.x, ll.y, ll.x + width(inst), ll.y + height(inst)};
}

double PlacementModel::dbuToMicron(int64_t value) const {
    return static_cast<double>(value) / static_cast<double>(dbu_per_micron);
}

std::string typeToString(InstanceType type) {
    switch (type) {
        case InstanceType::Cell:
            return "CELL";
        case InstanceType::Macro:
            return "MACRO";
        case InstanceType::Blockage:
            return "BLOCKAGE";
    }
    return "UNKNOWN";
}

InstanceType parseInstanceType(const std::string& text) {
    if (text == "CELL") {
        return InstanceType::Cell;
    }
    if (text == "MACRO") {
        return InstanceType::Macro;
    }
    if (text == "BLOCKAGE") {
        return InstanceType::Blockage;
    }
    throw PlacementError("unknown instance type: " + text);
}

bool overlaps(const Rect& a, const Rect& b) {
    return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

int64_t overlapArea(const Rect& a, const Rect& b) {
    int64_t x0 = std::max(a.x0, b.x0);
    int64_t y0 = std::max(a.y0, b.y0);
    int64_t x1 = std::min(a.x1, b.x1);
    int64_t y1 = std::min(a.y1, b.y1);
    if (x0 >= x1 || y0 >= y1) {
        return 0;
    }
    return (x1 - x0) * (y1 - y0);
}

bool contains(const Rect& outer, const Rect& inner) {
    return outer.x0 <= inner.x0 && outer.y0 <= inner.y0 && outer.x1 >= inner.x1 &&
           outer.y1 >= inner.y1;
}

int64_t snapUpToGrid(int64_t x, int64_t origin, int64_t step) {
    return origin + ceilDiv(x - origin, step) * step;
}

int64_t snapDownToGrid(int64_t x, int64_t origin, int64_t step) {
    return origin + floorDiv(x - origin, step) * step;
}

int64_t manhattan(const Point& a, const Point& b) {
    const int64_t dx = a.x >= b.x ? a.x - b.x : b.x - a.x;
    const int64_t dy = a.y >= b.y ? a.y - b.y : b.y - a.y;
    return dx + dy;
}

}  // namespace legalizer
