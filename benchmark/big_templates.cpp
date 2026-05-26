#include "defs.hpp"
#include "big.hpp"

template <int limbs>
u64 big_add(const config &conf) {
    big<limbs> x, y;
    conf.rng.fill(x.words, limbs);
    conf.rng.fill(y.words, limbs);

    for (u64 i = 0; i < conf.iters.add; i++) {
        x += y;
        asm volatile("":"+m"(x)::"memory");
    }

    auto checksum = hash(x.words, limbs);
    sink ^= checksum;
    return checksum;
}

template <int limbs>
u64 big_sub(const config &conf) {
    big<limbs> x, y;
    conf.rng.fill(x.words, limbs);
    conf.rng.fill(y.words, limbs);
    x.words[limbs-1] = -1;
    y.words[limbs-1] = 0;

    for (u64 i = 0; i < conf.iters.sub; i++) {
        x -= y;
        asm volatile("":"+m"(x)::"memory");
    }

    auto checksum = hash(x.words, limbs);
    sink ^= checksum;
    return checksum;
}

template <int limbs>
u64 big_mul(const config &conf) {
    big<limbs> x, y;
    conf.rng.fill(x.words, limbs);
    conf.rng.fill(y.words, limbs);
    y.words[0] |= 1;

    for (u64 i = 0; i < conf.iters.mul; i++) {
        x *= y;
        asm volatile("":"+m"(x)::"memory");
    }

    auto checksum = hash(x.words, limbs);
    sink ^= checksum;
    return checksum;
}

template <int limbs>
u64 big_binom(const config &conf) {
    u64 checksum = 0;
    auto [n, k] = conf.binom;
    big<limbs> result;

    for (u64 i = 0; i < conf.iters.binom; i++) {
        result = big<limbs>::binom(binom_n_for_iter(n, k, i), k);
        asm volatile("":"+m"(result)::"memory");
        // checksum ^= hash(result.words, limbs) ^ i;
    }

    checksum = hash(result.words, limbs);
    sink ^= checksum;
    return checksum;
}


template u64 big_add<LIMBS>(const config&);
template u64 big_sub<LIMBS>(const config&);
template u64 big_mul<LIMBS>(const config&);
template u64 big_binom<LIMBS>(const config&);
