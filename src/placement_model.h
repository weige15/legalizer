#ifndef PLACEMENT_MODEL_H
#define PLACEMENT_MODEL_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace legalizer {

using Coord = std::int64_t;

struct Point {
    Coord x = 0;
    Coord y = 0;
};

struct Rect {
    Coord lx = 0;
    Coord ly = 0;
    Coord ux = 0;
    Coord uy = 0;
};

enum class InstanceType {
    Cell,
    Macro,
    Blockage
};

struct Placement {
    Coord lx = 0;
    Coord ly = 0;
};

struct MovableCell {
    std::string name;
    Coord original_lx = 0;
    Coord original_ly = 0;
    Coord width = 0;
    Coord height = 0;
    std::size_t input_index = 0;
    std::optional<Placement> legal;
};

struct Obstacle {
    std::string name;
    Rect rect;
    InstanceType type = InstanceType::Macro;
};

struct Design {
    Coord dbu_per_micron = 0;
    Rect die;
    Coord site_width = 0;
    Coord site_height = 0;
    std::vector<MovableCell> cells;
    std::vector<Obstacle> obstacles;
};

Coord width(const Rect& rect);
Coord height(const Rect& rect);
Coord area(const Rect& rect);
bool isValid(const Rect& rect);
bool overlaps(const Rect& a, const Rect& b);
bool contains(const Rect& outer, const Rect& inner);
Rect intersection(const Rect& a, const Rect& b);
Point centerFloor(const Rect& rect);
Rect originalRect(const MovableCell& cell);
Rect placedRect(const MovableCell& cell);
Rect rectFromOrigin(const MovableCell& cell, Coord lx, Coord ly);
Coord manhattanDisplacementDbu(const MovableCell& cell, Coord lx, Coord ly);
bool isSiteAligned(Coord x, Coord die_lx, Coord site_width);
Coord alignUp(Coord value, Coord origin, Coord step);
Coord alignDown(Coord value, Coord origin, Coord step);
Coord ceilDiv(Coord num, Coord den);
void validateDesignMetadata(const Design& design);
void validatePositiveInstance(const std::string& name, Coord width, Coord height);
std::string instanceTypeName(InstanceType type);

} // namespace legalizer

#endif
