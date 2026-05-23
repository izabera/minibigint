OPTFLAGS = -O3 -march=native
CXXFLAGS = $(OPTFLAGS) $(SANITIZE)
override CXXFLAGS += -ggdb3 -std=c++23 -MMD -MP
CXX = clang++
LINK.o = $(CXX) $(LDFLAGS)

ifeq ($(origin HAVE_GMP_MULLO_N), undefined)
GMP_MULLO_PROBE = 'extern "C" void __gmpn_mullo_n(); int main() { __gmpn_mullo_n(); }'
HAVE_GMP_MULLO_N := $(shell echo $(GMP_MULLO_PROBE) | \
					$(LINK.o) -x c++ - -lgmp -o /dev/null 2>/dev/null; \
					[ $$? -eq 1 ]; echo $$?)
endif

override CPPFLAGS += -DHAVE_GMP_MULLO_N=$(HAVE_GMP_MULLO_N)

all: test benchmark/bench

test: OPTFLAGS =
test: SANITIZE = -fsanitize=address,undefined
test: LDFLAGS += $(SANITIZE)
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
