#include "big.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <initializer_list>
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
    using t = big<limbs>;
    t a, b;
    for (auto i = 0; i < 200; i++) {
        for (auto& w : a.words) w = u64(pcg.gen()) << 32 | pcg.gen();
        for (auto& w : b.words) w = u64(pcg.gen()) << 32 | pcg.gen();
        if (a < b) {
            auto tmp = a;
            a = b;
            b = tmp;
        }

        auto sum = a + b,
             mul = a * b,
             sub = a - b;

        auto validate = [&](auto& result, auto gmp, auto op, auto name) {
            op(gmp.words, a.words, b.words, limbs);
            if (memcmp(gmp.words, result.words, sizeof result.words)) {
                printf("%s fail - limbs=%d i=%d\n", name, limbs, i);
                printf("big: %s\n", print(sum));
                printf("gmp: %s\n", print(gmp));
                return false;
            }
            return true;
        };
        if (!validate(sum, big<limbs  >{}, mpn_add_n, "sum")) return false;
        if (!validate(mul, big<limbs*2>{}, mpn_mul_n, "mul")) return false;
        if (!validate(sub, big<limbs  >{}, mpn_sub_n, "sub")) return false;
        return true;
    }

    return true;
}

template <auto limbs>
bool test_arith_all() {
    if constexpr (limbs >= 4 && limbs <= 80)
        return test_arith_size<limbs>() && test_arith_all<limbs + 1>();
    return true;
}

template <auto limbs>
bool test_shift_size() {
    auto check = [](big<limbs> input, u64 shift) {
        auto result = input;
        result <<= shift;

        big<limbs> gmp{};
        mpz_t z;
        mpz_init(z);
        mpz_import(z, limbs, -1, sizeof input.words[0], 0, 0, input.words);
        mpz_mul_2exp(z, z, shift);
        mpz_fdiv_r_2exp(z, z, limbs * 64);
        mpz_export(gmp.words, nullptr, -1, sizeof gmp.words[0], 0, 0, z);

        auto ok = result == gmp;
        mpz_clear(z);

        if (!ok) {
            printf("shift fail - limbs=%d shift=%lu\n", int(limbs), shift);
            printf("in:  %s\n", print(input));
            printf("big: %s\n", print(result));
            printf("gmp: %s\n", print(gmp));
            return false;
        }
        return true;
    };

    big<limbs> zero{};
    big<limbs> one{1};
    big<limbs> high{};
    high.words[limbs - 1] = u64(1) << 63;

    // annoying boundary cases
    for (auto shift : {0, 1, 7, 31, 63, 64, 65, 127, 128,
                       limbs * 64 - 1, limbs * 64, limbs * 64 + 1})
        if (!check(zero, shift) || !check(one, shift) || !check(high, shift))
            return false;

    for (auto i = 0; i < 200; i++) {
        big<limbs> value;
        for (auto& w : value.words) w = u64(pcg.gen()) << 32 | pcg.gen();

        // bunch of random ones too
        for (auto shift : {u64(pcg.gen() & 127), u64(pcg.gen() % (limbs * 64 + 1))})
            if (!check(value, shift))
                return false;
    }

    return true;
}

template <auto limbs>
bool test_shift_all() {
    if constexpr (limbs >= 4 && limbs <= 80)
        return test_shift_size<limbs>() && test_shift_all<limbs + 1>();
    return true;
}

auto test_binoms() {
    auto check = []<auto limbs>(u64 n, u64 k) {
        auto result = big<limbs>::binom(n, k);
        big<limbs> gmp{};
        mpz_t z;
        mpz_init(z);
        mpz_bin_uiui(z, n, k);
        mpz_export(gmp.words, nullptr, -1, sizeof gmp.words[0], 0, 0, z);
        mpz_clear(z);

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
    if (!test_arith_all<4>() || !test_shift_all<4>() || !test_binoms()) {
        puts("fail");
        return 1;
    }

    puts("ok");
}
