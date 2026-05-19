#include "placement_model.h"

#include <cstdlib>
#include <stdexcept>

namespace legalizer {

Coord width(const Rect& rect) {
    return rect.ux - rect.lx;
}

Coord height(const Rect& rect) {
    return rect.uy - rect.ly;
}

Coord area(const Rect& rect) {
    if (!isValid(rect)) {
        return 0;
    }
    return width(rect) * height(rect);
}

bool isValid(const Rect& rect) {
    return rect.lx < rect.ux && rect.ly < rect.uy;
}

bool overlaps(const Rect& a, const Rect& b) {
    return a.lx < b.ux && b.lx < a.ux && a.ly < b.uy && b.ly < a.uy;
}

bool contains(const Rect& outer, const Rect& inner) {
    return outer.lx <= inner.lx && inner.ux <= outer.ux &&
           outer.ly <= inner.ly && inner.uy <= outer.uy;
}

Rect intersection(const Rect& a, const Rect& b) {
    Rect out{a.lx > b.lx ? a.lx : b.lx,
             a.ly > b.ly ? a.ly : b.ly,
             a.ux < b.ux ? a.ux : b.ux,
             a.uy < b.uy ? a.uy : b.uy};
    if (!isValid(out)) {
        return Rect{0, 0, 0, 0};
    }
    return out;
}

Point centerFloor(const Rect& rect) {
    return Point{(rect.lx + rect.ux) / 2, (rect.ly + rect.uy) / 2};
}

Rect originalRect(const MovableCell& cell) {
    return Rect{cell.original_lx, cell.original_ly,
                cell.original_lx + cell.width, cell.original_ly + cell.height};
}

Rect placedRect(const MovableCell& cell) {
    if (!cell.legal.has_value()) {
        throw std::runtime_error("cell has no legal placement: " + cell.name);
    }
    return rectFromOrigin(cell, cell.legal->lx, cell.legal->ly);
}

Rect rectFromOrigin(const MovableCell& cell, Coord lx, Coord ly) {
    return Rect{lx, ly, lx + cell.width, ly + cell.height};
}

Coord manhattanDisplacementDbu(const MovableCell& cell, Coord lx, Coord ly) {
    return std::llabs(lx - cell.original_lx) + std::llabs(ly - cell.original_ly);
}

bool isSiteAligned(Coord x, Coord die_lx, Coord site_width) {
    if (site_width <= 0) {
        return false;
    }
    Coord rem = (x - die_lx) % site_width;
    return rem == 0;
}

Coord alignUp(Coord value, Coord origin, Coord step) {
    if (step <= 0) {
        throw std::invalid_argument("alignment step must be positive");
    }
    Coord delta = value - origin;
    if (delta >= 0) {
        return origin + ((delta + step - 1) / step) * step;
    }
    return origin + (delta / step) * step;
}

Coord alignDown(Coord value, Coord origin, Coord step) {
    if (step <= 0) {
        throw std::invalid_argument("alignment step must be positive");
    }
    Coord delta = value - origin;
    if (delta >= 0) {
        return origin + (delta / step) * step;
    }
    return origin + ((delta - step + 1) / step) * step;
}

Coord ceilDiv(Coord num, Coord den) {
    if (den <= 0) {
        throw std::invalid_argument("ceilDiv denominator must be positive");
    }
    if (num <= 0) {
        return 0;
    }
    return (num + den - 1) / den;
}

void validateDesignMetadata(const Design& design) {
    if (design.dbu_per_micron <= 0) {
        throw std::runtime_error("DBU_Per_Micron must be positive");
    }
    if (design.site_width <= 0 || design.site_height <= 0) {
        throw std::runtime_error("site dimensions must be positive");
    }
    if (!isValid(design.die)) {
        throw std::runtime_error("die area must have positive width and height");
    }
}

void validatePositiveInstance(const std::string& name, Coord w, Coord h) {
    if (w <= 0 || h <= 0) {
        throw std::runtime_error("instance '" + name + "' has non-positive dimensions");
    }
}

std::string instanceTypeName(InstanceType type) {
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

} // namespace legalizer
