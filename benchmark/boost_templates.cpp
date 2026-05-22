#include "defs.hpp"
#include <boost/multiprecision/cpp_int.hpp>

namespace mp = boost::multiprecision;
template <int limbs>
using boost_uint = mp::number<mp::cpp_int_backend<
    limbs * 64,
    limbs * 64,
    mp::unsigned_magnitude,
    mp::unchecked,
    void>>;

template <int limbs> u64 boost_add(const config &conf) {
    boost_uint<limbs> x, y;
    conf.rng.fill(reinterpret_cast<u64*>(x.backend().limbs()), limbs);
    conf.rng.fill(reinterpret_cast<u64*>(y.backend().limbs()), limbs);

    for (u64 i = 0; i < conf.iters.add; i++) {
        x += y;
        asm volatile("":"+m"(x)::"memory");
    }

    auto checksum = hash(reinterpret_cast<const u64*>(x.backend().limbs()), limbs);
    sink ^= checksum;
    return checksum;
}

template <int limbs> u64 boost_mul(const config &conf) {
    boost_uint<limbs> x, y;
    conf.rng.fill(reinterpret_cast<u64*>(x.backend().limbs()), limbs);
    conf.rng.fill(reinterpret_cast<u64*>(y.backend().limbs()), limbs);

    for (u64 i = 0; i < conf.iters.add; i++) {
        x *= y;
        asm volatile("":"+m"(x)::"memory");
    }

    auto checksum = hash(reinterpret_cast<const u64*>(x.backend().limbs()), limbs);
    sink ^= checksum;
    return checksum;
}

template <int limbs> u64 boost_binom(const config &conf) {
    u64 checksum = 0;

    // this is just the basic recursive form
    // which is apparently the recommended way to do it
    // https://stackoverflow.com/a/33027607
    auto binomial = [](this auto&& self, u64 n, u64 k) -> boost_uint<limbs> {
        if (k == 0)
            return 1;
        return (n * self(n - 1, k - 1)) / k;
    };

    auto [n, k] = conf.binom;
    for (u64 i = 0; i < conf.iters.binom; i++) {
        auto value = binomial(binom_n_for_iter(n, k, i), k);
        checksum ^= hash(reinterpret_cast<const u64*>(value.backend().limbs()), limbs) ^ i;
    }

    sink ^= checksum;
    return checksum;
}

template u64 boost_add<LIMBS>(const config&);
template u64 boost_mul<LIMBS>(const config&);
template u64 boost_binom<LIMBS>(const config&);
