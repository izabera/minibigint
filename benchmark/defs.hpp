#pragma once

#define MAXLIMBS 80

#ifndef LIMBS
#define LIMBS MAXLIMBS
#endif

#include <cstdint>
using u32 = uint32_t;
using u64 = uint64_t;

// bunch of lightweight utilities
struct pcg {
    u64 state, inc = 99999;
    u32 gen() {
        auto old = state;
        state = old * 6364136223846793005 + inc;
        u32 xs = ((old >> 18) ^ old) >> 27;
        u32 rot = old >> 59;
        return (xs >> rot) | (xs << ((-rot) & 31));
    }
    u64 gen64() { return gen() | u64(gen()) << 32; }
    void fill(u64 *ptr, u64 size) {
        for (u64 i = 0; i < size; i++)
            ptr[i] = gen64();
    }
};

struct config {
    mutable pcg rng { u64(this) };

    u64 step = 4, rounds = 5;
    struct { u64 min, max; } limbs { 4, 80 };
    struct { u64 add, mul, binom; } iters { -1ul, -1ul, -1ul };
    struct { u64 n, k; } binom { -1ul, 255 };
    config(int argc, char **argv);
};

static inline auto fmix64(u64 k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccd;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53;
    k ^= k >> 33;
    return k;
}

static inline u64 hash(const u64 *ptr, u64 size) { // something like murmur3

    u64 acc = 0x6a09e667f3bcc909;
    for (u64 i = 0; i < size; i++)
        acc = fmix64(acc ^ ptr[i] ^ i * 0x9e3779b97f4a7c15);
    return acc;
}

static inline u64 binom_n_for_iter(u64 n, u64 k, u64 i) {
    u64 delta = n - k >= 15 ? (i & 15) : 0;
    return n - delta;
}

static volatile u64 sink;

struct buf {
    u64 data[MAXLIMBS];
    buf() : data{} {}
    buf(const config& conf, int limbs) { conf.rng.fill(data, limbs); }

    // basically a length limited copy assign
    auto copyfrom(const buf& other, int limbs) {
        __builtin_memcpy(data, other, sizeof *data * limbs);
    }

    operator const u64*() const { return data; }
    operator u64*() { return data; }
};

// lightweight templates for gmp just to pass the limbs parameter
template <int limbs> u64   gmp_add  (const config&);
template <int limbs> u64   gmp_mul  (const config&);
template <int limbs> u64   gmp_binom(const config&);

template <int limbs> u64 boost_add  (const config&);
template <int limbs> u64 boost_mul  (const config&);
template <int limbs> u64 boost_binom(const config&);

template <int limbs> u64   big_add  (const config&);
template <int limbs> u64   big_mul  (const config&);
template <int limbs> u64   big_binom(const config&);

#define ALL(X) \
    X( 4) X( 5) X( 6) X( 7) X( 8) X( 9) X(10) X(11) \
    X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) \
    X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27) \
    X(28) X(29) X(30) X(31) X(32) X(33) X(34) X(35) \
    X(36) X(37) X(38) X(39) X(40) X(41) X(42) X(43) \
    X(44) X(45) X(46) X(47) X(48) X(49) X(50) X(51) \
    X(52) X(53) X(54) X(55) X(56) X(57) X(58) X(59) \
    X(60) X(61) X(62) X(63) X(64) X(65) X(66) X(67) \
    X(68) X(69) X(70) X(71) X(72) X(73) X(74) X(75) \
    X(76) X(77) X(78) X(79) X(80)

#define X(n) \
extern template u64   gmp_add  <n>(const config&); \
extern template u64   gmp_mul  <n>(const config&); \
extern template u64   gmp_binom<n>(const config&); \
extern template u64 boost_add  <n>(const config&); \
extern template u64 boost_mul  <n>(const config&); \
extern template u64 boost_binom<n>(const config&); \
extern template u64   big_add  <n>(const config&); \
extern template u64   big_mul  <n>(const config&); \
extern template u64   big_binom<n>(const config&);

ALL(X)

#undef X
