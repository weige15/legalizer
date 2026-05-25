CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -Isrc

MODULE_SRCS := \
	src/placement_model.cpp \
	src/gp_parser.cpp \
	src/row_interval_builder.cpp \
	src/density_estimator.cpp \
	src/legalizer.cpp \
	src/tcl_writer.cpp

.PHONY: all clean

all: Legalizer

Legalizer: src/main.cpp $(MODULE_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	$(RM) Legalizer
