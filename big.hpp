#include <bit>
#include <cstdint>
#include <numeric>
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

    // we only need to support n choose k with k in 1..255
    // so we precompute a table of all the modular inverses and shifts
    static constexpr auto inverses() {
        struct {
            struct { u64 shift, inv; } values[256]; // AoS
        } modular;

        auto newton = [](auto d) {
            u64 x = d; // valid inverse mod 8 for odd d
            x *= 2 - d * x;
            x *= 2 - d * x;
            x *= 2 - d * x;
            x *= 2 - d * x;
            x *= 2 - d * x;
            x *= 2 - d * x;
            return x;
        };

        modular.values[0] = {0, 1};
        for (u64 i = 1; i < 256; i++) {
            u64 shift = std::countr_zero(i);
            modular.values[i] = {shift, (i >> shift) ? newton(i>>shift): 1};
        }

        return modular;
    }

    static constexpr big binom(u64 n, u64 k) {
        if (k > n || k > 255)
            return {};

        big C{1};
        auto last = 0; // avoid iterating over a ton of zeros
        constexpr auto invtable = inverses();

        // n choose i == (n choose (i-1)) * (n-i+1) / i
        for (u64 i = 1; i <= k; i++) {
            u64 num = n-i+1, den = i;
            auto g = std::gcd(num, den);
            num /= g;
            den /= g;

            // divide first, which is exact
            if (den != 1) {
                u64 carry = 0;

                auto [shift, inv] = invtable.values[den];
                auto odd = den >> shift;
                for (auto i = 0; i <= last || carry; i++) {
                    last = std::max(i, last);
                    u64 q = (C.words[i] - carry) * inv; // low 64 bits
                    auto prod = u128(q) * odd + carry;
                    C.words[i] = q;
                    carry = prod >> 64;
                }
                // division and shifts can reduce the last limb we're touching
                while (last > 0 && C.words[last] == 0)
                    --last;

                if (shift) {
                    u64 hi = 0;
                    for (int i = last; i >= 0; i--) {
                        auto w = C.words[i];
                        C.words[i] = (w >> shift) | (hi << (64 - shift));
                        hi = w;
                    }
                    while (last > 0 && C.words[last] == 0)
                        --last;
                }
            }

            // then multiply, so the state stays a bit smaller
            if (num != 1) {
                u64 carry = 0;
                for (auto i = 0; i <= last; i++) {
                    auto tmp = u128(C.words[i]) * num + carry;
                    C.words[i] = tmp;
                    carry = tmp >> 64;
                }
                // and multiplication can increase it
                if (carry)
                    C.words[++last] = carry;
            }
        }
        return C;
    }
};
