#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

#include "defs.hpp"

#define X(n) \
{ \
    {   gmp_add<n>,   gmp_sub<n>,   gmp_mul<n>,   gmp_binom<n>, }, \
    { boost_add<n>, boost_sub<n>, boost_mul<n>, boost_binom<n>, }, \
    {   big_add<n>,   big_sub<n>,   big_mul<n>,   big_binom<n>, }, \
},

#define MAXROUNDS 10
constexpr static auto inf = std::numeric_limits<double>::infinity();
struct times {
    double add = inf,
           sub = inf,
           mul = inf,
           binom = inf;
};
struct {
    struct {
        u64 (*add  )(const config&);
        u64 (*sub  )(const config&);
        u64 (*mul  )(const config&);
        u64 (*binom)(const config&);
        times all[MAXROUNDS]{}, best{}, mean{}, median{};
    } gmp, boost, big;
} static bench[] { {}, {}, {}, {}, ALL(X) };

#undef X


config::config(int argc, char **argv) {
    auto i = 1;

    auto usage = [&](auto err) {
        if (err)
            fprintf(stderr, "error: %s\n", argv[i]);

        fprintf(err == 0 ? stdout : stderr,
            "usage: bench [--min limbs] [--max limbs]\n"
            "             [--step limbs] [--rounds n] [--seed n]\n"
            "             [--add-iters n] [--sub-iters n] [--mul-iters n] [--binom-iters n]\n"
            "             [--binom-n n] [--binom-k k]\n"
            "             [--verbose n]\n"
            "             [--boost n]\n"
            "\n"
            "defaults: --min 4 --max 80\n"
            "          --step 4 --rounds 5 --seed 1234567\n"
            "          --binom-k 255\n"
            "          --boost %d\n"
            "          --verbose 0\n"
            "          auto select binom n based on k\n"
            "          auto select iters based on limbs\n", int(with_boost)
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
            !match("sub-iters"  , iters.sub  ) &&
            !match("mul-iters"  , iters.mul  ) &&
            !match("binom-iters", iters.binom) &&
            !match("binom-n"    , binom.n    ) &&
            !match("binom-k"    , binom.k    ) &&
            !match("boost"      , with_boost ) &&
            !match("verbose"    , verbose    ))
            usage(1);
    }

    if (i < argc)
        usage(std::string(argv[i]) == "--help" ? 0 : 1);
}

int main(int argc, char **argv) {
    auto conf = config(argc, argv);

    if (conf.binom.n == -1ul && conf.binom.k != 255ul) {
        fprintf(stderr, "you specified --binom-k without --binom-n\n");
        exit(1);
    }

    if (conf.step == 0) {
        fprintf(stderr, "invalid step\n");
        exit(1);
    }

    if (conf.rounds > MAXROUNDS || conf.rounds == 0)
        conf.rounds = MAXROUNDS;

    conf.with_boost = conf.with_boost && with_boost;

    if (conf.verbose) {
        printf("# boost=%d\n", int(conf.with_boost));

        printf("# step=%lu rounds=%lu seed=%lu limbs={%lu %lu}\n",
           conf.step, conf.rounds, conf.rng.state,
           conf.limbs.min, conf.limbs.max);

        puts("limbs,bits,op,big_ns,gmp_ns,gmp_x,boost_ns,boost_x,binom_n,binom_k");
    }

    for (auto i = conf.limbs.min; i <= conf.limbs.max; i += conf.step) {
        auto saved = conf;

        // this is approximately the biggest n choose k that fits
        if (conf.binom.n == -1ul && conf.binom.k == 255ul) {
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
        auto clamp = [](auto& var, u64 base, u64 calc) {
            if (var == -1ul)
                var = std::max(base, calc);
        };
        clamp(conf.iters.add  , 200'000, 50'000'000 / i);
        clamp(conf.iters.sub  , 200'000, 50'000'000 / i);
        clamp(conf.iters.mul  ,   1'000, 10'000'000 / (i * std::log2(i)));
        clamp(conf.iters.binom,   1'000, 10'000'000 / (i * (conf.binom.k ?: 1)));

        if (conf.verbose)
            printf("# iters={%lu %lu %lu %lu} binom={%lu %lu}\n",
               conf.iters.add, conf.iters.sub, conf.iters.mul, conf.iters.binom,
               conf.binom.n, conf.binom.k);

        for (u64 r = 0; r < conf.rounds; r++) {
            struct cksum { u64 add, sub, mul, binom; };
            auto timeit = [&](auto &impl) {
                conf.rng = saved.rng;

                cksum cksum;
                auto tick = std::chrono::steady_clock::now;
                auto t0 = tick();
                cksum.add   = impl.add  (conf); auto t1 = tick();
                cksum.sub   = impl.sub  (conf); auto t2 = tick();
                cksum.mul   = impl.mul  (conf); auto t3 = tick();
                cksum.binom = impl.binom(conf); auto t4 = tick();

                times current {
                    (t1-t0).count() * 1. / conf.iters.add  ,
                    (t2-t1).count() * 1. / conf.iters.sub  ,
                    (t3-t2).count() * 1. / conf.iters.mul  ,
                    (t4-t3).count() * 1. / conf.iters.binom,
                };

                impl.all[r].add   = current.add  ;
                impl.all[r].sub   = current.sub  ;
                impl.all[r].mul   = current.mul  ;
                impl.all[r].binom = current.binom;

                return cksum;
            };

            cksum ckbig{}, ckgmp{}, ckboost{};

            // run them in a random order
            enum { run_big, run_gmp, run_boost } order[] { run_big, run_gmp, run_boost };

            auto rng = saved.rng; // copy the rng so the runs see the same values
            rng.state ^= i; rng.gen();
            rng.state ^= r; rng.gen();

            std::shuffle(order, order + 3, rng);
            for (auto which : order) {
                switch (which) {
                    case run_big  : ckbig   = timeit(bench[i].big  ); break;
                    case run_gmp  : ckgmp   = timeit(bench[i].gmp  ); break;
                    case run_boost:
                        if (conf.with_boost)
                            ckboost = timeit(bench[i].boost);
                }
            }

            auto check = [&](const char *op, u64 b, u64 g, u64 x) {
                if (b == g && (!conf.with_boost || b == x))
                    return;

                fprintf(stderr,
                    "checksum mismatch! limbs=%lu op=%s "
                    "big=%016lx gmp=%016lx boost=%016lx\n",
                    i, op, b, g, x);
                exit(1);
            };

            check("add"  , ckbig.add  , ckgmp.add  , ckboost.add  );
            check("sub"  , ckbig.sub  , ckgmp.sub  , ckboost.sub  );
            check("mul"  , ckbig.mul  , ckgmp.mul  , ckboost.mul  );
            check("binom", ckbig.binom, ckgmp.binom, ckboost.binom);
        }

        auto stats = [&](auto& impl) {
            auto fieldstats = [&](double times::*field) {
                double vals[MAXROUNDS]{}, sum = 0;

                for (u64 r = 0; r < conf.rounds; r++) {
                    vals[r] = impl.all[r].*field;
                    sum += vals[r];
                }

                std::sort(vals, vals + conf.rounds);

                auto mid = conf.rounds / 2;
                impl.best  .*field = vals[0];
                impl.mean  .*field = sum / conf.rounds;
                impl.median.*field = conf.rounds % 2 ?
                                     vals[mid] :
                                    (vals[mid-1] + vals[mid]) / 2;
            };
            fieldstats(&times::add  );
            fieldstats(&times::sub  );
            fieldstats(&times::mul  );
            fieldstats(&times::binom);
        };
        stats(bench[i].big  );
        stats(bench[i].gmp  );
        stats(bench[i].boost);

#ifndef FIELD
#define FIELD median // pick the most interesting one between mean/median/best
#endif

        if (conf.with_boost) {
            if (conf.iters.add) {
                printf("%lu,%lu,add,%.3f,%.3f,%.3f,%.3f,%.3f\n", i, i * 64,
                    bench[i].big  .FIELD.add,
                    bench[i].gmp  .FIELD.add, bench[i].big.FIELD.add / bench[i].gmp  .FIELD.add,
                    bench[i].boost.FIELD.add, bench[i].big.FIELD.add / bench[i].boost.FIELD.add);
            }

            if (conf.iters.sub) {
                printf("%lu,%lu,sub,%.3f,%.3f,%.3f,%.3f,%.3f\n", i, i * 64,
                    bench[i].big  .FIELD.sub,
                    bench[i].gmp  .FIELD.sub, bench[i].big.FIELD.sub / bench[i].gmp  .FIELD.sub,
                    bench[i].boost.FIELD.sub, bench[i].big.FIELD.sub / bench[i].boost.FIELD.sub);
            }

            if (conf.iters.mul) {
                printf("%lu,%lu,mul,%.3f,%.3f,%.3f,%.3f,%.3f\n", i, i * 64,
                    bench[i].big  .FIELD.mul,
                    bench[i].gmp  .FIELD.mul, bench[i].big.FIELD.mul / bench[i].gmp  .FIELD.mul,
                    bench[i].boost.FIELD.mul, bench[i].big.FIELD.mul / bench[i].boost.FIELD.mul);
            }

            if (conf.iters.binom) {
                printf("%lu,%lu,binom,%.3f,%.3f,%.3f,%.3f,%.3f\n", i, i * 64,
                    bench[i].big  .FIELD.binom,
                    bench[i].gmp  .FIELD.binom, bench[i].big.FIELD.binom / bench[i].gmp  .FIELD.binom,
                    bench[i].boost.FIELD.binom, bench[i].big.FIELD.binom / bench[i].boost.FIELD.binom);
            }
        }
        else {
            if (conf.iters.add) {
                printf("%lu,%lu,add,%.3f,%.3f,%.3f,n/a,n/a\n", i, i * 64,
                    bench[i].big.FIELD.add,
                    bench[i].gmp.FIELD.add, bench[i].big.FIELD.add / bench[i].gmp.FIELD.add);
            }

            if (conf.iters.sub) {
                printf("%lu,%lu,sub,%.3f,%.3f,%.3f,n/a,n/a\n", i, i * 64,
                    bench[i].big.FIELD.sub,
                    bench[i].gmp.FIELD.sub, bench[i].big.FIELD.sub / bench[i].gmp.FIELD.sub);
            }

            if (conf.iters.mul) {
                printf("%lu,%lu,mul,%.3f,%.3f,%.3f,n/a,n/a\n", i, i * 64,
                    bench[i].big.FIELD.mul,
                    bench[i].gmp.FIELD.mul, bench[i].big.FIELD.mul / bench[i].gmp.FIELD.mul);
            }

            if (conf.iters.binom) {
                printf("%lu,%lu,binom,%.3f,%.3f,%.3f,n/a,n/a\n", i, i * 64,
                    bench[i].big.FIELD.binom,
                    bench[i].gmp.FIELD.binom, bench[i].big.FIELD.binom / bench[i].gmp.FIELD.binom);
            }
        }
        fflush(stdout);

        conf = saved;
    }
}
