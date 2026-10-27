#pragma once

#include <algorithm>   // for std::sort(), std::min()
#include <chrono>     // for steady_clock
#include <vector>   // for std::vector


template<typename T>
void keepitincompiledcode (T var) {  // forces the compiler to not delete or move "var" in compiler-optimised compiled code even if "var" is redundant (i.e. not used, doesn't affect visual outputs etc) cos the compiler tends to optimise away redundant code but we still need it for benchmarking
    asm volatile("" : : "r"(var) : "memory");
}


template<typename F>
std::vector<long long> run(F body, const int N) {

    constexpr int kWarmUp = 1000;
    std::vector<long long> samples;
    samples.reserve(N); //pre-allocate memory so that allocation of memory doesn't affect tests timings later on 

    for (int i = 0; i < kWarmUp; ++i) {
        body();
    }

    for (int i = 0; i < N; ++i) {
        const auto a = std::chrono::steady_clock::now();
        body();
        const auto b = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count());
    }
    
    std::sort(samples.begin(), samples.end());
    return samples;
}


// Non-template Functions in header file need to be inline if not linker will fail since every .cpp file that includes a header file gets its own copy of header's code (multiple definition of template is allowed, but not allowed for non-template Functions)
inline long long pct(const std::vector<long long>& s, double p) {
    size_t i = static_cast<size_t>(s.size() * p / 100.0);
    return s[std::min(i, s.size() - 1)]; //clamp to prevent p=100 overrunning
}


inline long long middle(std::vector<long long> v) {  // helper function
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

