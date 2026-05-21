#include "big.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <gmp.h>

using u32 = uint32_t;
using u64 = uint64_t;

struct {
    u64 state = 1234567890987654321, inc = 123456789;
    u32 gen() {
        auto old = state;
        state = old * 6364136223846793005 + inc;
        u32 xs = ((old >> 18) ^ old) >> 27;
        u32 rot = old >> 59;
        return (xs >> rot) | (xs << ((-rot) & 31));
    }
} static pcg;

static auto print = [](auto b) {
    constexpr auto w = sizeof b.words[0]*2;
    static char buf[sizeof b*3];
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

template <auto limbs>
bool test_arith_size() {
    big<limbs> a, b;
    for (auto i = 0; i < 200; i++) {
        for (auto& w : a.words) w = u64(pcg.gen()) << 32 | pcg.gen();
        for (auto& w : b.words) w = u64(pcg.gen()) << 32 | pcg.gen();

        big<limbs> sum = a + b, mul = a * b;

        big<limbs> gmpsum;
        mpn_add_n(gmpsum.words, a.words, b.words, limbs);
        if (memcmp(&sum.words, &gmpsum.words, sizeof sum.words)) {
            printf("sum fail - limbs=%d i=%d\n", limbs, i);
            printf("big: %s\n", print(sum));
            printf("gmp: %s\n", print(gmpsum));
            return false;
        }

        big<limbs*2> gmpmul;
        mpn_mul_n(gmpmul.words, a.words, b.words, limbs);
        if (memcmp(&mul.words, &gmpmul.words, sizeof mul.words)) {
            printf("mul fail - limbs=%d i=%d\n", limbs, i);
            printf("big: %s\n", print(mul));
            printf("gmp: %s\n", print(gmpmul));
            return false;
        }
    }

    return true;
}

template <auto limbs>
bool test_arith_all() {
    if constexpr (limbs >= 4 && limbs <= 80)
        return test_arith_size<limbs>() && test_arith_all<limbs + 1>();
    return true;
}

auto test_binoms() {
    auto check = []<auto limbs>(u64 n, u64 k) {
        auto result = big<limbs>::binom(n, k);
        big<limbs> gmp;
        mpz_t z;
        mpz_init(z);
        mpz_bin_uiui(z, n, k);
        mpz_export(gmp.words, nullptr, -1, sizeof gmp.words[0], 0, 0, z);
        if (result != gmp) {
            printf("binom fail - limbs=%d n=%lu k=%lu\n", int(limbs), n, k);
            printf("big: %s\n", print(result));
            printf("gmp: %s\n", print(gmp));
            return false;
        }
        return true;
    };

    for (u64 n = 0; n <= 260; ++n)
        for (u64 k = 0; k <= n && k <= 255; ++k)
            if (!check.operator()<4>(n, k))
                return false;

    for (u64 k : {1,2,3,7,8,31,50,100,127,255})
        if (!check.operator()<70>((1<<24)+255, k))
            return false;

    return true;
}

int main() {
    if (!test_arith_all<4>() || !test_binoms()) {
        puts("fail");
        return 1;
    }

    puts("ok");
}
