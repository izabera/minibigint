CXXFLAGS = -O3 -march=native -ggdb3 -std=c++23 -MMD -MP
# CXXFLAGS = -std=c++23 -MMD -MP -fsanitize=address
# LDFLAGS += -fsanitize=address
# CXXFLAGS = -std=c++23 -MMD -MP
CXX = clang++
LINK.o = $(CXX) $(LDFLAGS)

test: LDLIBS += -lgmp
test: test.o

clean:
	rm -f benchmark/*.[od] benchmark/bench *.[od] test

.PHONY: clean

limbs := $(shell seq 4 80)
objs := $(limbs:%=benchmark/bench_%.o)

benchmark/bench_%.o: CPPFLAGS += -DLIMBS=$* -I.
$(objs): benchmark/impl.cpp
	$(COMPILE.cpp) $< -o $@
benchmark/bench: LDLIBS += -lgmp
benchmark/bench: $(objs) benchmark/bench.o benchmark/gmp.o

-include *.d benchmark/*.d
