CXXFLAGS = -O3 -march=native -ggdb3 -std=c++23 -MMD -MP
CXX = clang++
LINK.o = $(CXX) $(LDFLAGS)

test: test.o

clean:
	rm -f *.[od] test

.PHONY: clean

-include *.d
