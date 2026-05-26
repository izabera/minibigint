#pragma once

#include <array>
#include <cstdint>
using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = __uint128_t;

namespace detail {

/*
  in the binomial we'll need to do this

  for p in primes
      for q in powers of p
          need = k/q
          for i in 1..need
              f *= inv[p]

  with k<=256 we can precalculate all possible powers and all values of need
  but obviously there are more powers that fit <=256 for small primes
  which is kind of annoying, you'd need to jump at variable offsets etc

  instead we can split things manually, and special case all the primes with
  more than 1 power <= 256:
  3 has 5 powers
  5 has 3 powers
  7, 11 and 13 have 2 powers
  everything else has 1

  for q in powers3
      need = need3[k][q]
      for i in 1..need
          f *= inv[3]

  repeat for power5, powers7, powers11, powers13

  and finally, the other 48 odd primes

  for p in rest of the primes
      need = need[k][p]
      for i in 1..need
          f *= inv[p]

  "wait, isn't that too many tables?  does it even fit in l1?"
  thanks for asking, yes it does:
  each need table uses 256 * number of powers of p
  so this part fits in 15872 bytes

  also, we will only access entries for the same k, so we group by k first
  the results is as follows
*/

constexpr inline std::array<u8,53> oddprimes {
          3,  5,  7, 11, 13, 17, 19, 23,
     29, 31, 37, 41, 43, 47, 53, 59, 61,
     67, 71, 73, 79, 83, 89, 97,101,103,
    107,109,113,127,131,137,139,149,151,
    157,163,167,173,179,181,191,193,197,
    199,211,223,227,229,233,239,241,251,
};
constexpr inline std::array<u8, 5> p3   { 3, 9, 27, 81, 243 };
constexpr inline std::array<u8, 3> p5   { 5, 25, 125 };
constexpr inline std::array<u8, 2> p7   { 7, 49 };
constexpr inline std::array<u8, 2> p11  { 11,121 };
constexpr inline std::array<u8, 2> p13  { 13,169 };
constexpr inline std::array<u8,48> rest {
     17, 19, 23, 29, 31, 37, 41, 43,
     47, 53, 59, 61, 67, 71, 73, 79,
     83, 89, 97,101,103,107,109,113,
    127,131,137,139,149,151,157,163,
    167,173,179,181,191,193,197,199,
    211,223,227,229,233,239,241,251,
};
struct needk { // XXX: maybe alignas(64) ?
    u8 need3 [p3  .size()];
    u8 need5 [p5  .size()];
    u8 need7 [p7  .size()];
    u8 need11[p11 .size()];
    u8 need13[p13 .size()];
    u8 needx [rest.size()];
    constexpr needk() {}
    constexpr needk(int k) {
        for (auto i = 0u; i < p3  .size(); i++) need3 [i] = k / p3  [i];
        for (auto i = 0u; i < p5  .size(); i++) need5 [i] = k / p5  [i];
        for (auto i = 0u; i < p7  .size(); i++) need7 [i] = k / p7  [i];
        for (auto i = 0u; i < p11 .size(); i++) need11[i] = k / p11 [i];
        for (auto i = 0u; i < p13 .size(); i++) need13[i] = k / p13 [i];
        for (auto i = 0u; i < rest.size(); i++) needx [i] = k / rest[i];
    }
};

constexpr inline auto table = [] {
    struct {
        needk needs[257];
        u64 inverses[oddprimes.size()];
    } table;

    for (auto i = 0; i < 257; i++)
        table.needs[i] = i;

    // newton
    for (auto i = 0u; i < oddprimes.size(); i++) {
        auto p = oddprimes[i];
        u64 x = p; // valid inverse mod 8 for odd p
        x *= 2 - p * x;
        x *= 2 - p * x;
        x *= 2 - p * x;
        x *= 2 - p * x;
        x *= 2 - p * x;
        x *= 2 - p * x;
        table.inverses[i] = x;
    }
    return table;
}();


// ughhhhhhhhhhh
// these can't even be templated easily because then you can't pass int
[[maybe_unused]] __attribute__((always_inline))
constexpr unsigned long addc(unsigned long x, unsigned long y,
                             unsigned long in, unsigned long *out) {
      return __builtin_addcl(x, y, in, out);
}

[[maybe_unused]] __attribute__((always_inline))
constexpr unsigned long long addc(unsigned long long x, unsigned long long y,
                                  unsigned long long in, unsigned long long *out) {
    return __builtin_addcll(x, y, in, out);
}
[[maybe_unused]] __attribute__((always_inline))
constexpr unsigned long subc(unsigned long x, unsigned long y,
                             unsigned long in, unsigned long *out) {
      return __builtin_subcl(x, y, in, out);
}

[[maybe_unused]] __attribute__((always_inline))
constexpr unsigned long long subc(unsigned long long x, unsigned long long y,
                                  unsigned long long in, unsigned long long *out) {
    return __builtin_subcll(x, y, in, out);
}
}

template <int n>
struct primeinfo {
    u8 powers[n];
    u8 need[256][n];
};
template <int limbs = 4>
struct big {
    u64 words[limbs]; // little endian

    constexpr big& operator+=(const big& other) {
        u64 carry = 0, i = 0;

        if consteval {
            for ( ; i < limbs; i++)
                words[i] = detail::addc(words[i], other.words[i], carry, &carry);
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

        #pragma GCC unroll limbs
        for ( ; i < (limbs & ~3); i += 4) {
            u64 r0 = rp[i+0];
            u64 r1 = rp[i+1];
            u64 r2 = rp[i+2];
            u64 r3 = rp[i+3];

            r0 = detail::addc(r0, other.words[i+0], carry, &carry);
            r1 = detail::addc(r1, other.words[i+1], carry, &carry);
            r2 = detail::addc(r2, other.words[i+2], carry, &carry);
            r3 = detail::addc(r3, other.words[i+3], carry, &carry);

            rp[i+0] = r0;
            rp[i+1] = r1;
            rp[i+2] = r2;
            rp[i+3] = r3;
        }

        #pragma GCC unroll limbs
        for (; i < limbs; i++)
            rp[i] = detail::addc(rp[i], other.words[i], carry, &carry);
        return *this;

        // (this is faster than gmp)
    }

    constexpr big& operator-=(const big& other) {
        u64 borrow = 0, i = 0;

        if consteval {
            for ( ; i < limbs; i++)
                words[i] = detail::subc(words[i], other.words[i], borrow, &borrow);
            return *this;
        }

        volatile u64* rp = words;

        #pragma GCC unroll limbs
        for ( ; i < (limbs & ~3); i += 4) {
            u64 r0 = rp[i+0];
            u64 r1 = rp[i+1];
            u64 r2 = rp[i+2];
            u64 r3 = rp[i+3];

            r0 = detail::subc(r0, other.words[i+0], borrow, &borrow);
            r1 = detail::subc(r1, other.words[i+1], borrow, &borrow);
            r2 = detail::subc(r2, other.words[i+2], borrow, &borrow);
            r3 = detail::subc(r3, other.words[i+3], borrow, &borrow);

            rp[i+0] = r0;
            rp[i+1] = r1;
            rp[i+2] = r2;
            rp[i+3] = r3;
        }

        #pragma GCC unroll limbs
        for (; i < limbs; i++)
            rp[i] = detail::subc(rp[i], other.words[i], borrow, &borrow);
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

                lo = detail::addc(lo, p_lo, 0, &c);
                hi = detail::addc(hi, p_hi, c, &c);
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
    constexpr bool operator<(const big& other) const {
        for (auto i = limbs-1; i >= 0; i--)
            if (words[i] != other.words[i])
                return words[i] < other.words[i];
        return false;
    }

    constexpr big operator+(const big& other) const {
        auto tmp = *this;
        tmp += other;
        return tmp;
    }
    constexpr big operator-(const big& other) const {
        auto tmp = *this;
        tmp -= other;
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
        if (k > n || k > 256)
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

        // we only need to support n choose k with k in 1..256
        // so we precompute a table of all the factors, without trailing zeros
        // their product is n choose k * k! / 2^v2
        u64 factors[256]{};
        for (auto i = 0u; i < k; i++) {
            auto f = n - k + 1 + i;
            factors[i] = f >> __builtin_ctzg(f);
        }

        // then remove all their factors in common with k!
        auto lo = n - k + 1;

        auto remove = [&] (u64 p, u64 inv, const auto& powers, const auto& needs) {
            if (p > k)
                return;
            for (u64 i = 0; i < powers.size(); i++) {
                u64 need = needs[i];
                if (!need)
                    break;
                u64 q = powers[i];
                u64 rem = lo % q;
                u64 m = lo + (rem ? q - rem : 0);
                for (auto j = 0u; j < need; j++, m += q) {
                    auto &f = factors[m - lo];
                    f *= inv; // f /= p
                }
            }
        };
        remove( 3, table.inverses[0], p3 , table.needs[k].need3 );
        remove( 5, table.inverses[1], p5 , table.needs[k].need5 );
        remove( 7, table.inverses[2], p7 , table.needs[k].need7 );
        remove(11, table.inverses[3], p11, table.needs[k].need11);
        remove(13, table.inverses[4], p13, table.needs[k].need13);

        for (auto i = 0u, base = 5u; i < rest.size(); i++) {
            u64 p = rest[i];
            if (p > k)
                break;
            u64 need = table.needs[k].needx[i];
            if (!need)
                break;
            u64 rem = lo % p;
            u64 m = lo + (rem ? p - rem : 0);
            u64 inv = table.inverses[i+base];
            for (auto j = 0u; j < need; j++, m += p) {
                auto &f = factors[m - lo];
                f *= inv;
            }
        }

        // multiply all the remaining factors up

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

        for (auto i = 0u; i < k; i++) {
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
        // since n < 2^64 and k<=256, the maximum v2 is 63, so whole == 0
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

static_assert(big<>{1337} + big<>{42} == big<>{1337+42});
static_assert(big<>{1337} - big<>{42} == big<>{1337-42});
static_assert(big<>{1337} * big<>{42} == big<>{1337*42});
