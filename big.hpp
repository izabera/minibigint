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

    static constexpr big binom(u64 n, u64 k) {
        if (k > n || k > 255)
            return {};

        big C{1};

        auto last = 0; // avoid iterating over a ton of zeros
        u64 num_acc = 1, den_acc = 1; // also batch things

        auto step = [&] {
            // divide first, which is exact
            if (den_acc != 1) {
                u64 carry = 0;

                auto inverse = [](auto i) {
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

                    u64 shift = std::countr_zero(i);
                    return std::pair{shift, (i >> shift) ? newton(i>>shift): 1};
                };

                auto [shift, inv] = inverse(den_acc);
                auto odd = den_acc >> shift;
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
            if (num_acc != 1) {
                u64 carry = 0;
                for (auto i = 0; i <= last; i++) {
                    auto tmp = u128(C.words[i]) * num_acc + carry;
                    C.words[i] = tmp;
                    carry = tmp >> 64;
                }
                // and multiplication can increase it
                if (carry)
                    C.words[++last] = carry;
            }
        };

        // n choose i == (n choose (i-1)) * (n-i+1) / i
        for (u64 i = 1; i <= k; i++) {
            u64 num = n-i+1, den = i;

            auto gcd_reduce = [](auto &a, auto &b) {
                auto g = std::gcd(a, b);
                a /= g;
                b /= g;
            };
            gcd_reduce(num, den);
            gcd_reduce(num, den_acc);
            gcd_reduce(den, num_acc);

            // batch as many num updates as will fit in a u64
            u64 tmpnum, tmpden;
            if (!__builtin_mul_overflow (num_acc, num, &tmpnum) && !__builtin_mul_overflow (den_acc, den, &tmpden)) {
                num_acc = tmpnum;
                den_acc = tmpden;
            }
            else {
                step();
                num_acc = num;
                den_acc = den;
            }
        }

        step(); // final flush
        return C;
    }
};
