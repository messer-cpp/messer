CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic-errors -O3 -march=native -Ilinse
LDFLAGS := -lboost_context -lstdc++fs
OBJS := messer include_dir.ipp


all: $(OBJS)

.PHONY: clean


messer: messer.cpp include_dir.ipp
	$(CXX) $(CXXFLAGS) -o$(@) $(<) $(LDFLAGS)

include_dir.ipp:
	echo | LC_ALL=C $(CPP) -xc++ -v - 2>&1 | awk '/<...>/,/^End/ {print}' | sed -n 's|^ \(.*\)|"\1",|p' > $(@)

clean:
	$(RM) $(OBJS)
