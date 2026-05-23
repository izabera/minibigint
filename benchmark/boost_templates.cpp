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

template <int limbs>
static auto make_boost(const config& conf) {
    boost_uint<limbs> x;
    auto& backend = x.backend();
    backend.resize(limbs, limbs); // should not actually allocate
    conf.rng.fill(reinterpret_cast<u64*>(backend.limbs()), limbs);
    backend.normalize();
    return x;
}

template <int limbs>
static u64 hash_boost(const boost_uint<limbs>& value) {
    auto& backend = value.backend();
    auto *words = reinterpret_cast<const u64*>(backend.limbs());
    return hash(words, limbs, backend.size());
}

template <int limbs>
u64 boost_add(const config &conf) {
    auto x = make_boost<limbs>(conf), y = make_boost<limbs>(conf);

    for (u64 i = 0; i < conf.iters.add; i++) {
        x += y;
        asm volatile("":"+m"(x)::"memory");
    }

    auto checksum = hash_boost<limbs>(x);
    sink ^= checksum;
    return checksum;
}

template <int limbs>
u64 boost_mul(const config &conf) {
    auto x = make_boost<limbs>(conf), y = make_boost<limbs>(conf) | 1;

    for (u64 i = 0; i < conf.iters.mul; i++) {
        x *= y;
        asm volatile("":"+m"(x)::"memory");
    }

    auto checksum = hash_boost<limbs>(x);
    sink ^= checksum;
    return checksum;
}

template <int limbs>
u64 boost_binom(const config &conf) {
    u64 checksum = 0;

    // this is just the basic recursive form
    // which is apparently the recommended way to do it
    // https://stackoverflow.com/a/33027607
    auto binomial = [](this auto&& self, u64 n, u64 k) -> boost_uint<limbs> {
        if (n - k < k)
            k = n - k;
        if (k == 0)
            return 1;
        return (n * self(n - 1, k - 1)) / k;
    };

    auto [n, k] = conf.binom;
    for (u64 i = 0; i < conf.iters.binom; i++) {
        auto value = binomial(binom_n_for_iter(n, k, i), k);
        checksum ^= hash_boost<limbs>(value) ^ i;
    }

    sink ^= checksum;
    return checksum;
}

template u64 boost_add<LIMBS>(const config&);
template u64 boost_mul<LIMBS>(const config&);
template u64 boost_binom<LIMBS>(const config&);
