#include "defs.hpp"
#include <gmp.h>

u64 gmp_add_impl(const config& conf, int limbs) {
    buf x(conf, limbs), y(conf, limbs);

    for (u64 i = 0; i < conf.iters.add; i++)
        mpn_add_n(x, x, y, limbs);

    auto checksum = hash(x, limbs);
    sink ^= checksum;
    return checksum;
}

u64 gmp_sub_impl(const config& conf, int limbs) {
    buf x(conf, limbs), y(conf, limbs);
    x[limbs-1] = -1;
    y[limbs-1] = 0;

    for (u64 i = 0; i < conf.iters.add; i++)
        mpn_sub_n(x, x, y, limbs);

    auto checksum = hash(x, limbs);
    sink ^= checksum;
    return checksum;
}


// my libgmp.so has this symbol, it's not public afaict
// i don't know how to access this otherwise
// if things break, use the fallback, which computes mullo manually
// mpn_mul_n computes both lo and hi which is not really a fair comparison
#if WITH_GMP_MULLO_N
extern "C"
void __gmpn_mullo_n(mp_ptr, mp_srcptr, mp_srcptr, mp_size_t);
#endif

u64 gmp_mul_impl(const config& conf, int limbs) {
    buf x(conf, limbs), y(conf, limbs), acc;
    y[0] |= 1;

    for (u64 i = 0; i < conf.iters.mul; i++) {
#if WITH_GMP_MULLO_N
        __gmpn_mullo_n(acc, x, y, limbs);
        x.copyfrom(acc, limbs);
#else
        mpn_mul_1(acc, y, limbs, x[0]);
        for (int j = 1; j < limbs; j++)
            mpn_addmul_1(acc + j, y, limbs - j, x[j]);
        x.copyfrom(acc, limbs);
#endif
    }

    auto checksum = hash(x, limbs);
    sink ^= checksum;
    return checksum;
}

u64 gmp_binom_impl(const config& conf, int limbs) {
    mpz_t value;
    mpz_init2(value, MAXLIMBS * 64); // XXX: or is it MAXLIMBS+1?
    u64 checksum = 0;

    auto [n, k] = conf.binom;
    for (u64 i = 0; i < conf.iters.binom; i++) {
        mpz_bin_uiui(value, binom_n_for_iter(n, k, i), k);
        checksum ^= hash(mpz_limbs_read(value), limbs, mpz_size(value)) ^ i;
    }

    mpz_clear(value);
    sink ^= checksum;
    return checksum;
}
