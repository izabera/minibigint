CXXFLAGS = -O3 -march=native -ggdb3 -std=c++23 -MMD -MP
# CXXFLAGS = -std=c++23 -MMD -MP -fsanitize=address
# LDFLAGS += -fsanitize=address
CXX = clang++
LINK.o = $(CXX) $(LDFLAGS)

test: LDLIBS += -lgmp
test: test.o

clean:
	rm -f *.[od] test

.PHONY: clean

-include *.d
