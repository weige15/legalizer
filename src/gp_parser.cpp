#include "gp_parser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace legalizer {
namespace {

bool parseIntToken(const std::string& token, Coord& out) {
    std::size_t pos = 0;
    try {
        long long value = std::stoll(token, &pos, 10);
        if (pos != token.size()) {
            return false;
        }
        out = static_cast<Coord>(value);
        return true;
    } catch (...) {
        return false;
    }
}

std::runtime_error lineError(int line_no, const std::string& message) {
    return std::runtime_error("line " + std::to_string(line_no) + ": " + message);
}

bool isBlank(const std::string& line) {
    for (char ch : line) {
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            return false;
        }
    }
    return true;
}

} // namespace

Design parseGpFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open input file: " + path);
    }

    Design design;
    bool have_dbu = false;
    bool have_ll = false;
    bool have_ur = false;
    bool have_site_w = false;
    bool have_site_h = false;
    bool have_header = false;

    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (!have_header && isBlank(line)) {
            continue;
        }

        std::istringstream iss(line);
        std::string first;
        if (!(iss >> first)) {
            continue;
        }

        if (!have_header) {
            if (first == "Name") {
                std::string llx;
                std::string lly;
                std::string width_token;
                std::string height_token;
                std::string type_token;
                if (!(iss >> llx >> lly >> width_token >> height_token >> type_token) ||
                    llx != "LLX" || lly != "LLY" || width_token != "Width" ||
                    height_token != "Height" || type_token != "Type") {
                    throw lineError(line_no, "expected header: Name LLX LLY Width Height Type");
                }
                std::string extra;
                if (iss >> extra) {
                    throw lineError(line_no, "unexpected token after instance header");
                }
                if (!have_dbu || !have_ll || !have_ur || !have_site_w || !have_site_h) {
                    throw lineError(line_no, "instance header appears before all metadata fields");
                }
                validateDesignMetadata(design);
                have_header = true;
                continue;
            }

            Coord a = 0;
            Coord b = 0;
            std::string t1;
            std::string t2;
            std::string extra;
            if (first == "DBU_Per_Micron") {
                if (!(iss >> t1) || !parseIntToken(t1, a) || (iss >> extra)) {
                    throw lineError(line_no, "malformed DBU_Per_Micron metadata");
                }
                design.dbu_per_micron = a;
                have_dbu = true;
            } else if (first == "DieArea_LL") {
                if (!(iss >> t1 >> t2) || !parseIntToken(t1, a) ||
                    !parseIntToken(t2, b) || (iss >> extra)) {
                    throw lineError(line_no, "malformed DieArea_LL metadata");
                }
                design.die.lx = a;
                design.die.ly = b;
                have_ll = true;
            } else if (first == "DieArea_UR") {
                if (!(iss >> t1 >> t2) || !parseIntToken(t1, a) ||
                    !parseIntToken(t2, b) || (iss >> extra)) {
                    throw lineError(line_no, "malformed DieArea_UR metadata");
                }
                design.die.ux = a;
                design.die.uy = b;
                have_ur = true;
            } else if (first == "Site_Width") {
                if (!(iss >> t1) || !parseIntToken(t1, a) || (iss >> extra)) {
                    throw lineError(line_no, "malformed Site_Width metadata");
                }
                design.site_width = a;
                have_site_w = true;
            } else if (first == "Site_Height") {
                if (!(iss >> t1) || !parseIntToken(t1, a) || (iss >> extra)) {
                    throw lineError(line_no, "malformed Site_Height metadata");
                }
                design.site_height = a;
                have_site_h = true;
            } else {
                throw lineError(line_no, "unknown metadata field '" + first + "'");
            }
            continue;
        }

        std::string sx;
        std::string sy;
        std::string sw;
        std::string sh;
        std::string type_token;
        if (!(iss >> sx >> sy >> sw >> sh >> type_token)) {
            throw lineError(line_no, "malformed instance row");
        }
        std::string extra;
        if (iss >> extra) {
            throw lineError(line_no, "unexpected token after instance row");
        }

        Coord lx = 0;
        Coord ly = 0;
        Coord w = 0;
        Coord h = 0;
        if (!parseIntToken(sx, lx) || !parseIntToken(sy, ly) ||
            !parseIntToken(sw, w) || !parseIntToken(sh, h)) {
            throw lineError(line_no, "instance geometry must be integer DBU values");
        }
        try {
            validatePositiveInstance(first, w, h);
        } catch (const std::exception& ex) {
            throw lineError(line_no, ex.what());
        }

        if (type_token == "CELL") {
            MovableCell cell;
            cell.name = first;
            cell.original_lx = lx;
            cell.original_ly = ly;
            cell.width = w;
            cell.height = h;
            cell.input_index = design.cells.size();
            design.cells.push_back(cell);
        } else if (type_token == "MACRO" || type_token == "BLOCKAGE") {
            Obstacle obstacle;
            obstacle.name = first;
            obstacle.rect = Rect{lx, ly, lx + w, ly + h};
            obstacle.type = (type_token == "MACRO") ? InstanceType::Macro : InstanceType::Blockage;
            design.obstacles.push_back(obstacle);
        } else {
            throw lineError(line_no, "unknown instance type '" + type_token + "'");
        }
    }

    if (!have_header) {
        throw std::runtime_error("missing instance table header");
    }
    return design;
}

} // namespace legalizer
