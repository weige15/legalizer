CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -Isrc

MODULE_SRCS := \
	src/placement_model.cpp \
	src/gp_parser.cpp \
	src/row_interval_builder.cpp \
	src/density_estimator.cpp \
	src/legalizer.cpp \
	src/tcl_writer.cpp

.PHONY: all test clean

all: Legalizer

Legalizer: src/main.cpp $(MODULE_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@

tests/test_legalizer: tests/test_legalizer.cpp $(MODULE_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: tests/test_legalizer Legalizer
	./tests/test_legalizer
	./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl

clean:
	$(RM) Legalizer
	$(RM) tests/test_legalizer
