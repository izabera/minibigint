#include <cstdint>
using u64 = uint64_t;
using u128 = __uint128_t;

template <int limbs = 4>
struct big {
    alignas(64) u64 words[limbs]{}; // little endian

    constexpr big& operator+=(const big& other) {
        u64 carry = 0;
#if 1
        for (auto i = 0; i < limbs; i++)
            words[i] = __builtin_addcl(words[i], other.words[i], carry, &carry);
#else
        for (auto i = 0; i < limbs; i++) {
            auto res = u128(words[i]) + other.words[i] + carry;
            words[i] = res;
            carry = res >> 64;
        }
#endif
        return *this;
    }

    constexpr big& operator*=(const big& other) {
        big acc;
        for (auto i = 0; i < limbs; i++) {
            u64 carry = 0;
            for (auto j = 0; i+j < limbs; j++) {
                auto tmp = u128(words[i]) * other.words[j] + carry + acc.words[i+j];
                acc.words[i+j] = tmp;
                carry = tmp >> 64;
            }
        }
        return *this = acc;
    }

    constexpr bool operator==(const big& other) const = default;
    constexpr bool operator!=(const big& other) const = default;

    constexpr big operator+(const big& other) const { auto tmp = *this; tmp += other; return tmp; }
    constexpr big operator*(const big& other) const { auto tmp = *this; tmp *= other; return tmp; }
};
