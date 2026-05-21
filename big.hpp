#include <cstdint>
using u64 = uint64_t;
using u128 = __uint128_t;

template <int limbs = 4>
struct big {
    alignas(64) u64 words[limbs]{}; // little endian

    constexpr big& operator+=(const big& other) {
        u64 carry = 0;
        #pragma clang loop unroll(full)
        for (auto i = 0; i < limbs; i++)
            words[i] = __builtin_addcl(words[i], other.words[i], carry, &carry);
        return *this;
    }

    constexpr big operator*(const big& other) const {
        big out;
        u64 carry0 = 0, carry1 = 0;
        for (auto i = 0; i < limbs; i++) {
            u64 lo = carry0, hi = carry1, top = 0;

            for (auto j = 0; j <= i; j++) {
                auto p = u128(words[j]) * other.words[i-j];
                u64 p_lo = p, p_hi = p >> 64, c = 0;

                lo = __builtin_addcl(lo, p_lo, 0, &c);
                hi = __builtin_addcl(hi, p_hi, c, &c);
                top += c;
            }

            out.words[i] = lo;
            carry0 = hi;
            carry1 = top;
        }
        return out;
    }

    constexpr bool operator==(const big& other) const = default;
    constexpr bool operator!=(const big& other) const = default;

    constexpr big operator+(const big& other) const {
        auto tmp = *this;
        tmp += other;
        return tmp;
    }
    constexpr big& operator*=(const big& other) {
        *this = *this * other;
        return *this;
    }
};
