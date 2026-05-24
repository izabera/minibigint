#pragma once

#include <cstdint>
using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = __uint128_t;

// this part is here to avoid reinstantiating it 300 times
namespace detail {
constexpr inline u32 oddprimes[] {
          3,  5,  7, 11, 13, 17, 19, 23,
     29, 31, 37, 41, 43, 47, 53, 59, 61,
     67, 71, 73, 79, 83, 89, 97,101,103,
    107,109,113,127,131,137,139,149,151,
    157,163,167,173,179,181,191,193,197,
    199,211,223,227,229,233,239,241,251,
};

constexpr inline auto inverses = [] {
    struct { u64 inverses[256]; } table{};

    // newton
    for (auto i = 3; i < 256; i += 2) {
        u64 x = i; // valid inverse mod 8 for odd d
        x *= 2 - i * x;
        x *= 2 - i * x;
        x *= 2 - i * x;
        x *= 2 - i * x;
        x *= 2 - i * x;
        x *= 2 - i * x;
        table.inverses[i] = x;
    }

    return table;
}();
}

// ughhhhhhhhhhh
[[maybe_unused]] __attribute__((always_inline))
constexpr unsigned long addc(unsigned long x, unsigned long y,
                             unsigned long c, unsigned long *out) {
      return __builtin_addcl(x, y, c, out);
}

[[maybe_unused]] __attribute__((always_inline))
constexpr unsigned long long addc(unsigned long long x, unsigned long long y,
                                  unsigned long long c,
                                  unsigned long long *out) {
    return __builtin_addcll(x, y, c, out);
}

template <int limbs = 4>
struct big {
    u64 words[limbs]; // little endian

    constexpr big& operator+=(const big& other) {
        u64 carry = 0, i = 0;

        if consteval {
            for ( ; i < limbs; i++)
                words[i] = addc(words[i], other.words[i], carry, &carry);
            return *this;
        }

        // both gcc and clang turn the loop above into a chain of
        // mov reg, [other+i*8]     # reg = other.words[i]
        // adc [this+i*8], reg      # words[i] += reg + carry
        // which seems optimal at a glance, but that adc is rmw
        //
        // on my test box (raptorlake), this is measurably slower than
        // mov reg, [this+i*8]      # reg = words[i]
        // adc reg, [other+i*8]     # reg += other.words[i] + carry
        // mov [this+i*8], reg      # words[i] = reg
        // which can be unrolled, rearranged, and pipelined better

        // unfortunately compilers really don't want to emit that
        // the only way i found is the following
        // https://godbolt.org/z/4zcrfh668
        volatile u64* rp = words; // volatile to force the order of loads

        #pragma clang loop unroll(full)
        for ( ; i < (limbs & ~3); i += 4) {
            u64 r0 = rp[i+0];
            u64 r1 = rp[i+1];
            u64 r2 = rp[i+2];
            u64 r3 = rp[i+3];

            r0 = addc(r0, other.words[i+0], carry, &carry);
            r1 = addc(r1, other.words[i+1], carry, &carry);
            r2 = addc(r2, other.words[i+2], carry, &carry);
            r3 = addc(r3, other.words[i+3], carry, &carry);

            rp[i+0] = r0;
            rp[i+1] = r1;
            rp[i+2] = r2;
            rp[i+3] = r3;
        }

        #pragma clang loop unroll(full)
        for (; i < limbs; i++)
            rp[i] = addc(rp[i], other.words[i], carry, &carry);
        return *this;

        // (this is faster than gmp)
    }

    constexpr big operator*(const big& other) const {
        big out;
        u64 carry0 = 0, carry1 = 0;
        for (auto i = 0; i < limbs; i++) {
            u64 lo = carry0, hi = carry1, top = 0;

            for (auto j = 0; j <= i; j++) {
                auto p = u128(words[j]) * other.words[i-j];
                u64 p_lo = p, p_hi = p >> 64, c = 0;

                lo = addc(lo, p_lo, 0, &c);
                hi = addc(hi, p_hi, c, &c);
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

    constexpr big& operator<<=(u64 x) {
        int whole = x >> 6;
        int frac = x & 63;

        if (whole) {
            for (auto i = limbs-1; i >= 0; i--)
                words[i] = i >= whole ? words[i - whole] : 0;
        }
        if (frac) {
            u64 carry = 0;
            for (auto i = 0; i < limbs; i++) {
                u64 x = words[i];
                words[i] = (x << frac) | carry;
                carry = x >> (64 - frac);
            }
        }
        return *this;
    }

    static constexpr big binom(u64 n, u64 k) {
        using namespace detail;

        if (n - k < k)
            k = n - k;
        if (k > n || k > 255)
            return {};

        switch (u64 tmp; k) {
            case 0: return {1};
            case 1: return {n};
            case 2: if (!__builtin_mul_overflow(n>>1, n&1?n:n-1, &tmp))
                        return {tmp};
        }

        big C;
        C.words[0] = 1;

        // https://en.wikipedia.org/wiki/Legendre's_formula
        // the max power of p that divides k! is sum(floor(k/p^i) for i in 1..inf)
        // the sum is finite because floor(k/p^i) is 0 if k<p^i

        // writing it in base 2 we get v2(k!) = k - popcount(k)

        // since n choose k == n! / (k!*(n-k)!) we have
        // v2(n choose k) == v2(n!) - v2(k!) - v2((n-k)!)
        //                == n - pop(n) - (k - pop(k)) - ((n-k) - pop(n-k))
        //                == pop(k) + pop(n-k) - pop(n)
        u64 v2 = __builtin_popcountg(k) + __builtin_popcountg(n-k) - __builtin_popcountg(n);

        // we only need to support n choose k with k in 1..255
        // so we precompute a table of all the factors, without trailing zeros
        // their product is n choose k * k! / 2^v2
        u64 factors[256];
        for (auto i = 0; i < k; i++) {
            auto f = n - k + 1 + i;
            factors[i] = f >> __builtin_ctzg(f);
        }

        // then remove all their factors in common with k!
        auto lo = n - k + 1;

        for (auto p : oddprimes) {
            if (p > k)
                break;

            // loop over the powers of p
            for (auto q = p; q <= k; q *= p) {
                auto need = k / q;

                // first multiple of q in [lo, n]
                auto rem = lo % q;
                auto m = lo + (rem ? q - rem : 0);

                for (auto i = 0; i < need; i++, m += q) {
                    auto &f = factors[m - lo];
                    // if (m - lo >= k) throw;

                    f *= inverses.inverses[p]; // f /= p
                }
            }
        }

        // multiply them all up

        int last = 0;
        u64 acc = 1; // batch things

        auto step = [&] {
            u64 carry = 0;
            for (auto i = 0; i <= last; i++) { // don't iterate over zeros
                auto tmp = u128(C.words[i]) * acc + carry;
                C.words[i] = tmp;
                carry = tmp >> 64;
            }
            if (carry)
                C.words[++last] = carry;
        };

        for (auto i = 0; i < k; i++) {
            auto f = factors[i];
            if (f <= 1)
                continue;
            u64 tmp;
            if (!__builtin_mul_overflow(acc, f, &tmp))
                acc = tmp;
            else {
                step();
                acc = f;
            }
        }

        step(); // final flush

        // then multiply by 2^v2
        // this is similar to operator<<=, with a few optimisations:
        // since n < 2^64 and k<256, the maximum v2 is 63, so whole == 0
        // also we know we can stop at last
        if (v2) {
            u64 carry = 0;
            for (auto i = 0; i <= last; i++) {
                u64 x = C.words[i];
                C.words[i] = (x << v2) | carry;
                carry = x >> (64 - v2);
            }
            if (carry)
                C.words[++last] = carry;
        }
        for (auto i = last + 1; i < limbs; i++)
            C.words[i] = 0;
        return C;
    }
};
