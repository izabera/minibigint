// gmp is not templated
// this file provides some wrappers to keep the same api as the rest
#include "defs.hpp"

u64 gmp_add_impl  (const config&, int limbs);
u64 gmp_mul_impl  (const config&, int limbs);
u64 gmp_binom_impl(const config&, int limbs);

template <int limbs> u64 gmp_add  (const config &conf) { return gmp_add_impl  (conf, limbs); }
template <int limbs> u64 gmp_mul  (const config &conf) { return gmp_mul_impl  (conf, limbs); }
template <int limbs> u64 gmp_binom(const config &conf) { return gmp_binom_impl(conf, limbs); }

template u64 gmp_add<LIMBS>(const config&);
template u64 gmp_mul<LIMBS>(const config&);
template u64 gmp_binom<LIMBS>(const config&);
