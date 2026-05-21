#include "big.hpp"

auto add(const big<> *__restrict a, const big<> *__restrict b) { return *a + *b; }
auto mul(const big<> *__restrict a, const big<> *__restrict b) { return *a * *b; }

template <auto limbs = 4>
struct bigprint {
    using big = big<limbs>;
    char data[sizeof big::words*2 + limbs]{};
    constexpr static auto w = sizeof big::words[0] * 2;

    constexpr bigprint(const big& b) {
        auto ptr = data;
        for (auto n : b.words) {
            for (int i = w-1; i >= 0; i--) {
                ptr[i] = "0123456789abcdef"[n & 0xf];
                n >>= 4;
            }
            ptr[w] = '_';
            ptr += w+1;
        }
        *--ptr = 0;
    }
};

#include <cstdio>
#include <cstring>
int main() {
    big a, b;
    memcpy(&a, "This is some long string to set the initial state for my numbers", sizeof b);
    // b={0,1,0,0};
    memcpy(&b, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+", sizeof b);
    printf("a: %s\n", bigprint(a).data);
    printf("b: %s\n", bigprint(b).data);
    big c = a * b;
    printf("c: %s\n", bigprint(c).data);
}
