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
gmpobjs   = $(limbs:%=benchmark/bench_gmp_%.o)
bigobjs   = $(limbs:%=benchmark/bench_big_%.o)
boostobjs = $(limbs:%=benchmark/bench_boost_%.o)
objs      = $(gmpobjs) $(bigobjs) $(boostobjs)

benchmark/bench_gmp_%.o:   CPPFLAGS += -I. -DLIMBS=$*
benchmark/bench_big_%.o:   CPPFLAGS += -I. -DLIMBS=$*
benchmark/bench_boost_%.o: CPPFLAGS += -I. -DLIMBS=$*

$(gmpobjs):   benchmark/gmp_templates.cpp
$(bigobjs):   benchmark/big_templates.cpp
$(boostobjs): benchmark/boost_templates.cpp

$(objs):
	$(COMPILE.cpp) $< -o $@
benchmark/bench: LDLIBS += -lgmp
benchmark/bench: $(objs) benchmark/bench.o benchmark/gmp.o

-include *.d benchmark/*.d
