#include <cstdint>
using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = __uint128_t;

// this part is here to avoid reinstantiating it 300 times
namespace detail {
constexpr static u32 primes[] {
      2,  3,  5,  7, 11, 13, 17, 19, 23,
     29, 31, 37, 41, 43, 47, 53, 59, 61,
     67, 71, 73, 79, 83, 89, 97,101,103,
    107,109,113,127,131,137,139,149,151,
    157,163,167,173,179,181,191,193,197,
    199,211,223,227,229,233,239,241,251,
};

constexpr static auto inverses = [] {
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

template <int limbs = 4>
struct big {
    u64 words[limbs]{}; // little endian

    constexpr big& operator+=(const big& other) {
        u64 carry = 0, i = 0;

        volatile u64* rp = words; // this absolutely has to live in a register

        #pragma clang loop unroll(full)
        for ( ; i < (limbs & ~3); i += 4) {
            u64 r0 = rp[i+0];
            u64 r1 = rp[i+1];
            u64 r2 = rp[i+2];
            u64 r3 = rp[i+3];

            r0 = __builtin_addcl(r0, other.words[i+0], carry, &carry);
            r1 = __builtin_addcl(r1, other.words[i+1], carry, &carry);
            r2 = __builtin_addcl(r2, other.words[i+2], carry, &carry);
            r3 = __builtin_addcl(r3, other.words[i+3], carry, &carry);

            rp[i+0] = r0;
            rp[i+1] = r1;
            rp[i+2] = r2;
            rp[i+3] = r3;
        }

        #pragma clang loop unroll(full)
        for (; i < limbs; i++)
            rp[i] = __builtin_addcl(rp[i], other.words[i], carry, &carry);
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

        big C{1};

        // we only need to support n choose k with k in 1..255
        // so we precompute a table of all the factors
        // their product is n choose k * k!
        u64 factors[256];
        for (auto i = 0; i < k; i++)
            factors[i] = n - k + 1 + i;

        // then remove all their factors in common with k!
        auto lo = n - k + 1;

        for (auto p : primes) {
            if (p > k)
                break;

            // loop over the powers of p
            for (auto q = p; q <= k; q *= p) {
                auto need = k / q;

                // first multiple of q in [lo, n]
                auto m = lo + ((q - lo % q) % q);

                for (auto i = 0; i < need; i++, m += q) {
                    auto &f = factors[m - lo];
                    // if (m - lo >= k) throw;

                    // f /= p
                    if (p == 2)
                        f >>= 1;
                    else
                        f *= inverses.inverses[p];
                }
            }
        }

        // finally, multiply them all up

        u64 acc = 1; // batch things
        auto step = [&, last = 0] mutable {
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
        return C;
    }
};
