#include "defs.hpp"
#include "big.hpp"

template <int limbs> u64 big_add  (const config &conf) { return {}; }
template <int limbs> u64 big_mul  (const config &conf) { return {}; }
template <int limbs> u64 big_binom(const config &conf) { return {}; }

template u64 big_add<LIMBS>(const config&);
template u64 big_mul<LIMBS>(const config&);
template u64 big_binom<LIMBS>(const config&);
