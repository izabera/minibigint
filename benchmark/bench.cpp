#include <cmath>
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
        struct { double add, mul, binom; } times {
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity(),
        };
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

    if (conf.binom.n == -1 && conf.binom.k != 255) {
        fprintf(stderr, "you specified --binom-k without --binom-n\n");
        exit(1);
    }

    puts("limbs,bits,op,big_ns,gmp_ns,gmp_x,boost_ns,boost_x,binom_n,binom_k");
    for (auto i = conf.limbs.min; i <= conf.limbs.max; i+= conf.step) {
        auto saved = conf;

        // this is approximately the biggest n choose k that fits
        if (conf.binom.n == -1 && conf.binom.k == 255) {
            // not exact but it's close enough
            auto fits = [&] (u64 n, u64 k) {
                double estimate = 0;
                for (u64 i = 1; i <= k; i++)
                    estimate += std::log2(double(n - i + 1)) - std::log2(double(i));
                return estimate < i * 64;
            };

            u64 lo = conf.binom.k; // k choose k = 1 so it fits

            for (u64 hi = -1ul; lo < hi; ) {
                u64 mid = lo + (hi - lo + 1) / 2;

                if (fits(mid, conf.binom.k))
                    lo = mid;
                else
                    hi = mid - 1;
            }

            conf.binom.n = lo;
        }

        // these are pretty much arbitrary
        if (conf.iters.add == -1)
            conf.iters.add = std::max(200'000ul, 50'000'000 / i);
        if (conf.iters.mul == -1)
            conf.iters.mul = std::max(1'000ul, 10'000'000 / i);
        if (conf.iters.binom == -1)
            conf.iters.binom = std::max(100ul, 5'000'000 / (i * (conf.binom.k ?: 1)));

        printf("# conf: step=%lu rounds=%lu "
               "limbs={%lu %lu} iters{%lu %lu %lu} binom{%lu %lu}\n",
               conf.step, conf.rounds,
               conf.limbs.min, conf.limbs.max,
               conf.iters.add, conf.iters.mul, conf.iters.binom,
               conf.binom.n, conf.binom.k);

        auto timeit = [&](auto &impl) {
            conf.rng = saved.rng;

            struct { u64 add, mul, binom; } cksum;

            auto t0 = std::chrono::steady_clock::now();
            cksum.add   = impl.add(conf);   auto t1 = std::chrono::steady_clock::now();
            cksum.mul   = impl.mul(conf);   auto t2 = std::chrono::steady_clock::now();
            cksum.binom = impl.binom(conf); auto t3 = std::chrono::steady_clock::now();

            decltype(impl.times) current {
                (t1-t0).count() * 1. / conf.iters.add,
                (t2-t1).count() * 1. / conf.iters.mul,
                (t3-t2).count() * 1. / conf.iters.binom,
            };

            // keep the best of n runs
            impl.times.add   = std::min(impl.times.add  , current.add  );
            impl.times.mul   = std::min(impl.times.mul  , current.mul  );
            impl.times.binom = std::min(impl.times.binom, current.binom);

            return cksum;
        };

        for (u64 r = 0; r < conf.rounds; r++) {
            auto big   = timeit(bench[i].big);
            auto gmp   = timeit(bench[i].gmp);
            auto boost = timeit(bench[i].boost);

            auto check = [&](const char *op, u64 b, u64 g, u64 x) {
                if (b == g && b == x)
                    return;

                fprintf(stderr,
                    "checksum mismatch! limbs=%lu op=%s "
                    "big=%016lx gmp=%016lx boost=%016lx\n",
                    i, op, b, g, x);
                exit(1);
            };

            check("add"  , big.add  , gmp.add  , boost.add  );
            check("mul"  , big.mul  , gmp.mul  , boost.mul  );
            check("binom", big.binom, gmp.binom, boost.binom);
        }

//               i,bits,op, big, gmp,  x, boost,  x,   n,  k
        printf("%lu,%lu,add,%.3f,%.3f,%.3f,%.3f,%.3f,n/a,n/a\n",
                i, i * 64,
                bench[i].big  .times.add,
                bench[i].gmp  .times.add, bench[i].big.times.add / bench[i].gmp  .times.add,
                bench[i].boost.times.add, bench[i].big.times.add / bench[i].boost.times.add);

        printf("%lu,%lu,mul,%.3f,%.3f,%.3f,%.3f,%.3f,n/a,n/a\n",
                i, i * 64,
                bench[i].big  .times.mul,
                bench[i].gmp  .times.mul, bench[i].big.times.mul / bench[i].gmp  .times.mul,
                bench[i].boost.times.mul, bench[i].big.times.mul / bench[i].boost.times.mul);

        printf("%lu,%lu,binom,%.3f,%.3f,%.3f,%.3f,%.3f,%lu,%lu\n",
                i, i * 64,
                bench[i].big  .times.binom,
                bench[i].gmp  .times.binom, bench[i].big.times.binom / bench[i].gmp  .times.binom,
                bench[i].boost.times.binom, bench[i].big.times.binom / bench[i].boost.times.binom,
                conf.binom.n, conf.binom.k);
        fflush(stdout);

        conf = saved;
    }
}
