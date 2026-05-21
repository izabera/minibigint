#include "big.hpp"
#include <gmp.h>
#include <cstdio>

using u32 = uint32_t;

struct {
    u64 state = 1234567890987654321, inc = 123456789;
    u32 gen() {
        auto old = state;
        state = old * 6364136223846793005 + inc;
        u32 xs = ((old >> 18) ^ old) >> 27;
        u32 rot = old >> 59;
        return (xs >> rot) | (xs << ((-rot) & 31));
    }
} pcg;

template <auto limbs>
bool test_size() {
    big<limbs> a, b;
    for (auto i = 0; i < 200; i++) {
        for (auto& w : a.words) w = u64(pcg.gen()) << 32 | pcg.gen();
        for (auto& w : b.words) w = u64(pcg.gen()) << 32 | pcg.gen();

        big<limbs> sum = a + b, mul = a * b;
        big<limbs> gmpsum, gmpmul;

        auto print = [](auto b) {
            constexpr auto w = sizeof b.words[0]*2;
            static char buf[w*limbs+limbs+1];
            auto ptr = buf;
            for (auto n : b.words) {
                for (auto j = w; j > 0; j--) {
                    ptr[j-1] = "0123456789abcdef"[n & 0xf];
                    n >>= 4;
                }
                ptr[w] = '_';
                ptr += w+1;
            }
            return buf;
        };

        mpn_add_n(gmpsum.words, a.words, b.words, limbs);
        if (sum != gmpsum) {
            printf("sum fail - limbs=%d i=%d\n", limbs, i);
            printf("big: %s\n", print(sum));
            printf("gmp: %s\n", print(gmpsum));
            return false;
        }

        mpn_mul_n(gmpmul.words, a.words, b.words, limbs);
        if (mul != gmpmul) {
            printf("mul fail - limbs=%d i=%d\n", limbs, i);
            printf("big: %s\n", print(mul));
            printf("gmp: %s\n", print(gmpmul));
            return false;
        }
    }

    return true;
}

template <auto limbs>
bool test_all() {
    if constexpr (limbs >= 4 && limbs <= 80)
        return test_size<limbs>() && test_all<limbs + 1>();
    return true;
}

int main() {
    if (!test_all<4>()) {
        puts("fail");
        return 1;
    }
    puts("ok");
}
