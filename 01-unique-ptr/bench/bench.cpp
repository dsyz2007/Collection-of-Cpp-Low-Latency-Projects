#include "../Unique.h"

#include <vector>
#include <algorithm>   //for std::sort
#include <chrono>   // for timing
#include <cstdio>   // for printf
#include <memory>   // for std::make_unique, std::make_shared
#include <functional>    // for std::function<void()> 


struct Node { 
    long long f[28]; 
};

static_assert(sizeof(Node) == 224);


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


long long pct(const std::vector<long long>& s, double p) {
    size_t i = static_cast<size_t>(s.size() * p / 100.0);
    return s[std::min(i, s.size() - 1)]; //clamp to prevent p=100 overrunning
}



long long middle(std::vector<long long> v) {  // helper function
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}


int main(){

    constexpr int N = 100000;

    std::vector< std::function<void()> > s;  // data type needs to store lambdas but every lambda has its own type so the solution is to use std::function<void()> which is a type that can hold any callable taking nothing and returning nothing (it hides each lambda's real type behind one shared type and makes the call through a pointer at runtime)
    s.reserve(7);
    
    auto empty = []{};  // empty lambda

    auto raw = []{ 
        Node* a = new Node{};
        a->f[0] = 12345;
        keepitincompiledcode(a->f);
        delete a; 
    };

    auto rawNoZero = []{  // same as raw, but "new Node" (no braces) skips zeroing the 224 bytes: shows what the zeroing alone costs
        Node* a = new Node;
        a->f[0] = 12345;
        keepitincompiledcode(a->f);
        delete a;
    };

    auto mine = []{  // using my own imple of unique pointer
        Unique a = make_unique<Node>();
        a->f[0] = 12345;
        keepitincompiledcode(a.get());       
    };

    auto stdUnique = []{  // using std::make_unique (C++'s imple of unique pointer)
        auto a = std::make_unique<Node>();
        a->f[0] = 12345;
        keepitincompiledcode(a.get());
    };

    auto makeShared = []{  // using std::make_shared (C++'s imple of shared pointer)
        auto a = std::make_shared<Node>();
        a->f[0] = 12345;
        keepitincompiledcode(a.get());
    };

    auto sharedNew = []{  // using std::shared_ptr (C++'s imple of shared pointer) + New (this is expected to have worse performance compared to makeShared cos double allocations instead of just one needed)
        std::shared_ptr<Node> a(new Node());
        a->f[0] = 12345;
        keepitincompiledcode(a.get());
    };
    

    s.push_back(empty);
    s.push_back(raw);
    s.push_back(mine);
    s.push_back(stdUnique);
    s.push_back(makeShared);
    s.push_back(sharedNew);
    s.push_back(rawNoZero);


    constexpr int ROUNDS = 5;
    constexpr int V = 7;
    const char* names[V] = {"empty (timer only)", "raw new Node{}", "Unique (mine)", "std::unique_ptr",
                            "std::make_shared", "shared_ptr(new)", "raw new Node (no zeroing)"};
    std::vector<long long> p50[V], p99[V], p999[V];  // one list per variant, one value per round

    // every round runs all 7 variants in the same order, so each one gets a share of whatever
    // speed the machine happens to be running at (it drifts a lot on this Chromebook)
    for (int r = 0; r < ROUNDS; ++r) {
        for (int v = 0; v < V; ++v) {
            std::vector<long long> samples = run(s[v], N);
            p50[v].push_back(pct(samples, 50));
            p99[v].push_back(pct(samples, 99));
            p999[v].push_back(pct(samples, 99.9));
        }
    }

    // -O2 optimisation turned on. Medians of the 5 rounds, in ns
    std::printf("%-26s %6s %6s %7s\n", "variant", "p50", "p99", "p99.9");
    for (int v = 0; v < V; ++v) {
        std::printf("%-26s %6lld %6lld %7lld\n", names[v], middle(p50[v]), middle(p99[v]), middle(p999[v]));
    }

}

