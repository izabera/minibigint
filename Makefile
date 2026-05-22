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
backends := gmp big boost

define bench_template
$(1)objs := $(limbs:%=benchmark/bench_$(1)_%.o)

$(limbs:%=benchmark/bench_$(1)_%.o): benchmark/bench_$(1)_%.o: benchmark/$(1)_templates.cpp
	$$(COMPILE.cpp) -I. -DLIMBS=$$* $$< -o $$@
endef

$(foreach backend,$(backends),$(eval $(call bench_template,$(backend))))

objs := $(foreach backend,$(backends),$($(backend)objs))

benchmark/bench: LDLIBS += -lgmp
benchmark/bench: $(objs) benchmark/bench.o benchmark/gmp.o

-include *.d benchmark/*.d
