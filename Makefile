CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

SRC := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
OBJ := $(SRC:.cpp=.o)
MAIN_OBJ := src/main.o
TEST_OBJ := tests/test_legalizer.o

.PHONY: all clean test

all: Legalizer

Legalizer: $(OBJ) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

tests/test_legalizer: $(OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -I. -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I. -c -o $@ $<

test: tests/test_legalizer
	./tests/test_legalizer

clean:
	rm -f Legalizer src/*.o tests/*.o tests/test_legalizer

