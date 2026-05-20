#include "tcl_writer.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace legalizer {

void writeTcl(const PlacementModel& model, const std::vector<Point>& placements,
              const std::string& output_path) {
    std::ostringstream text;
    text << std::setprecision(12);
    for (size_t id : model.cell_ids) {
        if (id >= placements.size()) {
            throw PlacementError("writer missing placement for " + model.instances[id].name);
        }
        text << "place_cell -inst_name " << model.instances[id].name << " -orient R0 -origin {"
             << model.dbuToMicron(placements[id].x) << " " << model.dbuToMicron(placements[id].y)
             << "}\n";
    }
    const std::string content = text.str();
    if (content.empty() && !model.cell_ids.empty()) {
        throw PlacementError("writer generated empty TCL");
    }
    if (content.find("detailed_placement") != std::string::npos) {
        throw PlacementError("writer generated forbidden detailed_placement command");
    }

    const std::string tmp = output_path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            throw PlacementError("unable to open temporary output: " + tmp);
        }
        out << content;
        out.flush();
        if (!out) {
            throw PlacementError("failed while writing temporary output: " + tmp);
        }
    }
    if (std::rename(tmp.c_str(), output_path.c_str()) != 0) {
        throw PlacementError("failed to rename temporary output to " + output_path);
    }
}

}  // namespace legalizer
