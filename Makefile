CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic-errors -O3 -march=native -Ilinse
LDFLAGS := -lboost_context -lstdc++fs
OBJS := messer


all: $(OBJS)

.PHONY: clean


messer: messer.cpp
	$(CXX) $(CXXFLAGS) -o$(@) $(<) $(LDFLAGS)

clean:
	$(RM) $(OBJS)
