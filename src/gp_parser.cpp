#include "gp_parser.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>

namespace legalizer {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) {
        ++a;
    }
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
        --b;
    }
    return s.substr(a, b - a);
}

[[noreturn]] void parseFail(int line, const std::string& reason) {
    throw PlacementError("parse error at line " + std::to_string(line) + ": " + reason);
}

int64_t parseI64(const std::string& text, int line, const std::string& field) {
    if (text.empty()) {
        parseFail(line, "missing integer field " + field);
    }
    char* end = nullptr;
    errno = 0;
    long long v = std::strtoll(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        parseFail(line, "invalid integer for " + field + ": " + text);
    }
    return static_cast<int64_t>(v);
}

void expectMeta(const std::string& line, int line_no, const std::string& key,
                std::vector<int64_t>* values) {
    std::istringstream iss(line);
    std::string got;
    iss >> got;
    if (got != key) {
        parseFail(line_no, "expected " + key);
    }
    std::string token;
    while (iss >> token) {
        values->push_back(parseI64(token, line_no, key));
    }
    std::string extra;
    if (iss.fail() && !iss.eof()) {
        parseFail(line_no, "malformed metadata for " + key);
    }
}

Rect makeRect(int64_t x, int64_t y, int64_t w, int64_t h, int line) {
    if (w <= 0 || h <= 0) {
        parseFail(line, "instance dimensions must be positive");
    }
    if (x > std::numeric_limits<int64_t>::max() - w ||
        y > std::numeric_limits<int64_t>::max() - h) {
        parseFail(line, "instance coordinate overflow");
    }
    return Rect{x, y, x + w, y + h};
}

}  // namespace

PlacementModel parseGpFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw PlacementError("unable to open input file: " + path);
    }

    PlacementModel model;
    std::string line;
    int line_no = 0;

    auto nextLine = [&]() -> std::string {
        if (!std::getline(in, line)) {
            parseFail(line_no + 1, "unexpected end of file");
        }
        ++line_no;
        return trim(line);
    };

    std::vector<int64_t> vals;
    vals.clear();
    expectMeta(nextLine(), line_no, "DBU_Per_Micron", &vals);
    if (vals.size() != 1) parseFail(line_no, "DBU_Per_Micron expects one value");
    model.dbu_per_micron = vals[0];

    vals.clear();
    expectMeta(nextLine(), line_no, "DieArea_LL", &vals);
    if (vals.size() != 2) parseFail(line_no, "DieArea_LL expects two values");
    model.die.x0 = vals[0];
    model.die.y0 = vals[1];

    vals.clear();
    expectMeta(nextLine(), line_no, "DieArea_UR", &vals);
    if (vals.size() != 2) parseFail(line_no, "DieArea_UR expects two values");
    model.die.x1 = vals[0];
    model.die.y1 = vals[1];

    vals.clear();
    expectMeta(nextLine(), line_no, "Site_Width", &vals);
    if (vals.size() != 1) parseFail(line_no, "Site_Width expects one value");
    model.site_width = vals[0];

    vals.clear();
    expectMeta(nextLine(), line_no, "Site_Height", &vals);
    if (vals.size() != 1) parseFail(line_no, "Site_Height expects one value");
    model.site_height = vals[0];

    std::string header = nextLine();
    if (header.empty()) {
        header = nextLine();
    }
    if (header != "Name LLX LLY Width Height Type") {
        parseFail(line_no, "expected column header");
    }

    while (std::getline(in, line)) {
        ++line_no;
        std::string t = trim(line);
        if (t.empty()) {
            continue;
        }
        std::istringstream iss(t);
        std::string name;
        std::string sx;
        std::string sy;
        std::string sw;
        std::string sh;
        std::string stype;
        std::string extra;
        if (!(iss >> name >> sx >> sy >> sw >> sh >> stype) || (iss >> extra)) {
            parseFail(line_no, "instance record must have six fields");
        }
        Instance inst;
        inst.name = name;
        inst.original = makeRect(parseI64(sx, line_no, "LLX"), parseI64(sy, line_no, "LLY"),
                                 parseI64(sw, line_no, "Width"),
                                 parseI64(sh, line_no, "Height"), line_no);
        try {
            inst.type = parseInstanceType(stype);
        } catch (const PlacementError&) {
            parseFail(line_no, "unknown type: " + stype);
        }
        inst.input_order = model.instances.size();
        model.instances.push_back(inst);
    }

    model.validateBasic();
    model.rebuildIndexes();
    return model;
}

}  // namespace legalizer
