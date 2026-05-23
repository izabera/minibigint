#include <string>
#include <cstdio>
#include <cstdlib>
#include <chrono>

#include "defs.hpp"

#define X(n) \
{ \
    {   gmp_add<n>,   gmp_mul<n>,   gmp_binom<n>, }, \
    { boost_add<n>, boost_mul<n>, boost_binom<n>, }, \
    {   big_add<n>,   big_mul<n>,   big_binom<n>, }, \
},

struct {
    struct {
        u64 (*add  )(const config&);
        u64 (*mul  )(const config&);
        u64 (*binom)(const config&);
        struct { double add, mul, binom; } times;
    } gmp, boost, big;
} bench[] { {}, {}, {}, {}, ALL(X) };

#undef X



config::config(int argc, char **argv) {
    auto i = 1;

    auto usage = [&](auto err) {
        if (err)
            fprintf(stderr, "error: %s\n", argv[i]);

        fprintf(err == 0 ? stdout : stderr,
            "usage: bench [--min limbs] [--max limbs]\n"
            "             [--step limbs] [--rounds n] [--seed n]\n"
            "             [--add-iters n] [--mul-iters n] [--binom-iters n]\n"
            "             [--binom-n n] [--binom-k k]\n"
            "\n"
            "defaults: --min 4 --max 80\n"
            "          --step 4 --rounds 5 --seed 1234567\n"
            "          --binom-k 255\n"
            "          auto select binom n based on k\n"
            "          auto select iters based on limbs\n"
        );
        exit(err);
    };

    for ( ; i+1 < argc; i+=2) {
        if (std::string(argv[i]) == "--help")
            usage(0);

        auto match = [&](auto name, auto &var) {
            std::string opt = argv[i];
            if (opt.starts_with("--") && opt.substr(2) == name) {
                var = std::stoul(argv[i+1]);
                return true;
            }
            return false;
        };
        if (!match("seed"       , rng.state  ) &&
            !match("step"       , step       ) &&
            !match("rounds"     , rounds     ) &&
            !match("min"        , limbs.min  ) &&
            !match("max"        , limbs.max  ) &&
            !match("add-iters"  , iters.add  ) &&
            !match("mul-iters"  , iters.mul  ) &&
            !match("binom-iters", iters.binom) &&
            !match("binom-n"    , binom.n    ) &&
            !match("binom-k"    , binom.k    ))
            usage(1);
    }

    if (i < argc)
        usage(std::string(argv[i]) == "--help" ? 0 : 1);
}

int main(int argc, char **argv) {
    auto conf = config(argc, argv);

    puts("limbs,bits,op,big_ns,gmp_ns,gmp_x,boost_ns,boost_x,binom_n,binom_k");
    for (auto i = 4; i <= MAXLIMBS; i++) {
        auto timeit = [&](auto &impl) {
            auto t0 = std::chrono::steady_clock::now();
            impl.add(conf);   auto t1 = std::chrono::steady_clock::now();
            impl.mul(conf);   auto t2 = std::chrono::steady_clock::now();
            impl.binom(conf); auto t3 = std::chrono::steady_clock::now();
            impl.times = {
                (t1-t0).count() / 1e9,
                (t2-t1).count() / 1e9,
                (t3-t2).count() / 1e9,
            };
        };
        timeit(bench[i].big);
        timeit(bench[i].gmp);
        timeit(bench[i].boost);

//              i bits,op,big, gmp,  x   boost x    n   k
        printf("%d,%d,add,%.3f,%.3f,%.3f,%.3f,%.3f,n/a,n/a\n",
                i, i * 64,
                bench[i].big  .times.add,
                bench[i].gmp  .times.add, bench[i].big.times.add / bench[i].gmp  .times.add,
                bench[i].boost.times.add, bench[i].big.times.add / bench[i].boost.times.add);

        printf("%d,%d,mul,%.3f,%.3f,%.3f,%.3f,%.3f,n/a,n/a\n",
                i, i * 64,
                bench[i].big  .times.mul,
                bench[i].gmp  .times.mul, bench[i].big.times.mul / bench[i].gmp  .times.mul,
                bench[i].boost.times.mul, bench[i].big.times.mul / bench[i].boost.times.mul);

        printf("%d,%d,binom,%.3f,%.3f,%.3f,%.3f,%.3f,%lu,%lu\n",
                i, i * 64,
                bench[i].big  .times.binom,
                bench[i].gmp  .times.binom, bench[i].big.times.binom / bench[i].gmp  .times.binom,
                bench[i].boost.times.binom, bench[i].big.times.binom / bench[i].boost.times.binom,
                conf.binom.n, conf.binom.k);
    }
}
