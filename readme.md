```
# build
make -j20

# basic sanity tests
./test

# benchmark 4..80 limbs
benchmark/bench --step 1 --rounds 3 | tee results.csv

python3 plot.py results.csv
```
