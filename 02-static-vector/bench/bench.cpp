#include "../static_vector.h"
#include "../../common/bench.h"

#include <array>
#include <cstdio>          // printf
#include <cstdlib>         // malloc, free
#include <functional>      // for std::function<void()> 
#include <new>             // std::bad_alloc
#include <vector>
#include <sys/resource.h>  // getrusage


// The allocation counter (static keeps g_allocs private to this file)
static long long g_allocs = 0;   // heap allocations made through operator new

void* operator new(std::size_t n) {   // we replaced the "new" and "delete" operator functions at file scope which overloads (replaces) the global new/delete for c++ programs
    ++g_allocs;
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc{};
}

// why 2 "delete" overloads: since C++14, the compiler may or may not call a "sized" version that is also tolds size (e.g. std::vector uses the "sized" version)  
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }


static long minor_faults() {
    rusage u{};
    getrusage(RUSAGE_SELF, &u);
    return u.ru_minflt;    // page faults so far in this process
}


constexpr int kN = 1000; // declared at file scope so can be used by lambdas without capturing anything

int main() {
    constexpr int kSamples = 10000;
    constexpr int kRounds = 5;
    constexpr int V = 5;

    auto empty = []{};

    auto array = []{
        std::array<int, kN> a;
        for (int i = 0; i < kN; ++i) {
            a[i] = i;
        }
        keepitincompiledcode(a.data());
    };

    auto staticvec = []{
        static_vector<int, kN> v;
        for (int i = 0; i < kN; ++i) {
            v.try_emplace_back(i);
        }
        keepitincompiledcode(v.begin());
    };

    auto growingnormalvec = []{
        std::vector<int> v;
        for (int i = 0; i < kN; ++i) {
            v.push_back(i);
        }
        keepitincompiledcode(v.data());
    };

    auto reservevec = []{
        std::vector<int> v;
        v.reserve(kN);
        for (int i = 0; i < kN; ++i) {
            v.push_back(i);
        }
        keepitincompiledcode(v.data());
    };

    std::vector< std::function<void()> > s;  // data type needs to store lambdas but every lambda has its own type so the solution is to use std::function<void()> which is a type that can hold any callable taking nothing and returning nothing (it hides each lambda's real type behind one shared type and makes the call through a pointer at runtime)
    s.reserve(V);
    s.push_back(empty);
    s.push_back(array);
    s.push_back(staticvec);
    s.push_back(growingnormalvec);
    s.push_back(reservevec);


    long cold[V], warm[V];
    long long allocs[V];
    for (int v = 0; v < V; ++v) { long before = minor_faults(); s[v](); cold[v] = minor_faults() - before; }
    for (int v = 0; v < V; ++v) { long before = minor_faults(); s[v](); warm[v] = minor_faults() - before; }
    for (int v = 0; v < V; ++v) { g_allocs = 0; s[v](); allocs[v] = g_allocs; }

    const char* names[V] = {"empty (timer only)", "std::array", "static_vector", "vector, growing", "vector + reserve"};
    std::vector<long long> p50[V], p99[V], p999[V];  // one list per variant, one value per round

    // every round runs all 5 variants in the same order, so each one gets a share of whatever
    // speed the machine happens to be running at (it drifts a lot on this Chromebook)
    for (int r = 0; r < kRounds; ++r) {
        for (int v = 0; v < V; ++v) {
            std::vector<long long> samples = run(s[v], kSamples);
            p50[v].push_back(pct(samples, 50));
            p99[v].push_back(pct(samples, 99));
            p999[v].push_back(pct(samples, 99.9));
        }
    }

    // -O2 optimisation turned on. Medians of the 5 rounds, in ns
    std::printf("%-26s %6s %6s %7s %15s %15s %15s\n", "variant", "p50", "p99", "p99.9", "allocations", "coldfaults", "warmfaults");
    for (int v = 0; v < V; ++v) {
        std::printf("%-26s %6lld %6lld %7lld %15lld %15ld %15ld\n", names[v], middle(p50[v]), middle(p99[v]), middle(p999[v]), allocs[v], cold[v], warm[v]);
    }
}
