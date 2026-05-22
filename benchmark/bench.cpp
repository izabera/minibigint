#include <string>
#include <cstdio>
#include <cstdlib>

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

    for (auto &b : bench) {
        b.big.add(conf);
        b.gmp.add(conf);
        b.boost.add(conf);

        b.big.mul(conf);
        b.gmp.mul(conf);
        b.boost.mul(conf);

        b.big.binom(conf);
        b.gmp.binom(conf);
        b.boost.binom(conf);
    }
}
