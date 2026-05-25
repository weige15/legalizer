CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -Isrc

MODULE_SRCS := \
	src/placement_model.cpp \
	src/gp_parser.cpp \
	src/row_interval_builder.cpp \
	src/density_estimator.cpp \
	src/legalizer.cpp \
	src/tcl_writer.cpp

CASE ?= public/ispd15_mgc_matrix_mult_a/
FLOW_LOG ?= flow_openroad.log
ANALYSIS_LOG ?= analysis_openroad.log

.PHONY: all test flow analysis clean

all: Legalizer

Legalizer: src/main.cpp $(MODULE_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@

tests/test_legalizer: tests/test_legalizer.cpp $(MODULE_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: tests/test_legalizer Legalizer
	./tests/test_legalizer
	./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl

flow:
	@FINAL_TABLES_ONLY=1 CASE_NAME="$(CASE)" openroad -exit flow.tcl > "$(FLOW_LOG)" 2>&1
	@awk '/^Performance Metrics$$/,/^----------------------------------------$$/' "$(FLOW_LOG)"
	@echo "Full OpenROAD log: $(FLOW_LOG)"

analysis:
	@FINAL_TABLES_ONLY=1 CASE_NAME="$(CASE)" openroad -exit analysis_detail.tcl > "$(ANALYSIS_LOG)" 2>&1
	@awk '/^Detailed Placement Analysis Metrics$$/,/^----------------------------------------$$/' "$(ANALYSIS_LOG)"
	@echo "Full OpenROAD log: $(ANALYSIS_LOG)"

clean:
	$(RM) Legalizer
	$(RM) tests/test_legalizer
