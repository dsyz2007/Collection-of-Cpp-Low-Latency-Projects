# static_vector: a vector that never touches the heap

A fixed-capacity vector in C++23. `static_vector<T, N>` keeps room for `N` elements inside the object itself, so it never allocates, and it builds elements only when you add them. I wrote it to learn how to manage object lifetimes by hand: raw aligned storage, building objects in place, ending their lives without freeing anything, and keeping all of that correct when a constructor throws halfway through a copy. Then I benchmarked it against `std::array` and `std::vector` to see what not allocating actually buys you.

```cpp
static_vector<Tracer, 4> v;                  // room for 4 Tracers, none built yet
Tracer* p = v.try_emplace_back(1, 2.0);      // built in place: 1 construction, no copies or moves
if (!v.try_emplace_back(2, 0.5)) { /* full: you get nullptr, not an exception */ }

for (const Tracer& t : v) { /* plain pointers work as iterators */ }
v.pop_back();                                // the last Tracer is destroyed right here
```

Adding to a full vector returns `nullptr` instead of throwing, because a hot path should never pay for exceptions. By the way, C++26's `std::inplace_vector` has a `try_emplace_back` with the same shape.

## Results

Each variant fills a container with 1,000 `int`s. The container is created, filled and destroyed inside the timed code, so one sample is one complete fill. 10,000 samples per variant, `-O2`, 5 rounds in which every variant takes a turn, medians of the 5. I ran it three times and threw the first run away, because the first run of a session is usually slower on this machine. The table is runs 2 and 3.

| Variant | p50 | p99 | heap allocations per fill |
|---|---|---|---|
| timer only (nothing between the two clock reads) | 27 ns | 29 ns | 0 |
| `std::array<int, 1000>`, filled by index | 164 ns | 166–169 ns | 0 |
| `static_vector<int, 1000>` | 868–869 ns | 984–985 ns | 0 |
| `std::vector<int>` + `reserve(1000)` | 2,350–2,351 ns | 2,594–2,732 ns | 1 |
| `std::vector<int>`, growing | 2,751–2,753 ns | 3,181–3,246 ns | 11 |

Some interesting stuff i found and learnt:

**I thought `static_vector` would be about 2× faster than `vector` + `reserve` "because it never has to resize".** It did came out 2.7× faster, but not for that reason. After `reserve`, the vector doesn't resize either. It makes one allocation, and growing from empty with 11 allocations only costs about 400 ns more than that, so a single allocation is worth a few tens of nanoseconds. The real difference is in the loop the compiler generated. For every `push_back`, GCC writes the vector's end pointer out to the stack and then reads it straight back, so each element waits on a round trip through memory. My loop keeps `size_` in a register the whole way and writes it back once at the end. I haven't worked out why GCC keeps the vector's pointers in memory there, but the gap is that, not the heap.

**I thought it would be as fast as a plain array, since neither allocates.** The array is actually 5.3× faster. Its loop is vectorised: one SSE store writes four `int`s at a time, so it goes round a 6-instruction loop 250 times. Mine stores one `int` per trip and checks `size_ == N` on every one of the 1,000 trips, and that check is what stops GCC vectorising it, even though it can never be true in this loop. So "no allocation" doesn't mean "as fast as an array". What decides it is how much work happens per element.

Page faults turned out not to matter. I counted them with `getrusage` around a single fill: the first time anything touches fresh memory it faults (3 times for the growing vector, once for the array's stack page), and after that every fill reuses the same memory and the count is 0. The gaps in the table all come from code, none from the kernel.


## The same instructions, 50% slower

While checking these numbers, the `static_vector` loop compiled to byte-for-byte identical instructions in two builds and ran about 1,100 ns in one and about 1,625 ns in the other. The only difference was where the loop sat in memory. In the slow build, the jump at the bottom of the loop ended exactly on a 32-byte boundary. Since a 2019 microcode update, Intel CPUs of this generation can't run a jump like that from their cache of already-decoded instructions (the "JCC erratum"), so a tight loop ending there slows down. Rebuilding with `-Wa,-mbranches-within-32B-boundaries`, which pads the code so no jump touches a boundary, brought it back.

That flag isn't a free speed-up, though. In the build the table comes from, it left `static_vector` alone and moved the `std::array` loop instead, the same 6 instructions, from 164 ns to 238 ns. On this CPU, a tight loop's speed can depend on its address by as much as half, and an unrelated edit somewhere else in the file can move it. I now compare variants inside one binary, and check any surprising gap with a second build before I believe it.

## How it works

**The storage is bytes, not `T`s.** It's `alignas(T) std::byte storage_[sizeof(T) * N]` plus a `size_`. An array of `T` would construct all `N` elements up front: with `T data_[N]`, an empty `static_vector<Tracer, 4>` built and destroyed 4 Tracers. `std::byte` has no constructor, so the bytes are just room, and `alignas(T)` makes sure the first one sits at an address a `T` is allowed to live at. Elements go in with `std::construct_at` and come out with `std::destroy_at`. Neither one allocates or frees anything.

**The constructor is `{}`, not `= default`.** With `= default`, writing `static_vector<int, 1000> v{};` asks for value-initialisation, and GCC zeroed all 4,008 bytes with a `rep stos` before anything was in them. A constructor I wrote myself, even an empty one, turns that into a single store that sets `size_` to 0. `= default` also stops a `const static_vector` from compiling at all.

**Finding slot `i`: do the arithmetic in bytes, then cast.** `storage_ + i * sizeof(T)`, cast to `T*`. Casting first and then adding `i * sizeof(T)` multiplies by `sizeof(T)` twice, because `T*` arithmetic already moves in whole `T`s. I tried it on purpose to experiment: the Tracers landed 256, 512 and 768 bytes away, far outside the object, and neither the normal build nor AddressSanitizer noticed.

**Build the element first before counting it.** `try_emplace_back` constructs into `slot(size_)` and only then does `++size_`. If the constructor throws, `size_` never claimed an object that doesn't exist. With the two lines swapped, the vector reported 3 elements while only 2 existed, and the destructor freed a string that had already been freed (`attempting double-free`).

**`clear()` skips the loop for types with nothing to destroy.** It uses `if constexpr (!std::is_trivially_destructible_v<T>)`, so for `int` the loop isn't even compiled and `clear()` is two instructions. In fairness, GCC at `-O2` produced the same two instructions without the `if constexpr` too, because it can see that destroying an `int` does nothing. Here it's a guarantee written into the code rather than a speed-up.

**The compiler's copy constructor would be wrong.** `= default` copies `storage_` as bytes and never runs `T`'s copy constructor. Tracer's counters showed 0 copies and 6 destructions for 3 constructions, and with `std::string` elements both vectors pointed at the same heap buffer and freed it twice. So all five special members are written by hand, element by element. Moving leaves the source empty, as a moved-from `std::vector` is.

**A copy that throws halfway has to clean up after itself.** If the third element's copy throws, the first two are already built, and the new vector's destructor never runs, because its constructor never finished. Nothing else knows those two elements exist. The copy constructor wraps its loop in `try { ... } catch (...) { clear(); throw; }`. Without that, LeakSanitizer reported 50 bytes leaked in 2 objects. The move constructor does the same, but only when `T`'s move can throw. When it can't, the move constructor is `noexcept`, and GCC rejects a `throw` inside it (`'throw' will always call 'terminate'`). So that part sits inside another `if constexpr`.

## Testing

Everything builds with `-Wall -Wextra -Werror`, in three builds: a normal one, AddressSanitizer + UBSan, and ThreadSanitizer. The tests use two helper types from `../common/`. `Tracer` counts its constructions, copies, moves, assignments and destructions, so every test checks exact numbers. `Thrower` can be told to throw on its *n*th construction, and its payload is a long `std::string` so a leak shows up under ASan.

The tests cover:

- filling to capacity and getting `nullptr` back
- `std::forward` copying an lvalue argument and moving an rvalue, with one copy and one move exactly
- `pop_back()` and `clear()` destroying elements immediately, not at the end of the scope
- writing through `operator[]`
- range-for over a `const` reference with zero copies
- the address spacing check
- all four copy and move operations, including self-assignment
- a constructor throwing inside `try_emplace_back`
- a copy constructor throwing on its third element

Two `static_assert`s check the conditional `noexcept`: moving a `static_vector<Tracer, 4>` can't throw, and moving a `static_vector<Thrower, 4>` can.

I also broke the code on purpose to check that the tests notice: The `= default` copy fails the Tracer counts and double-frees under ASan, `++size_` before the construct double-frees, and taking out the `try`/`catch` fails the throwing-copy test and shows up in LeakSanitizer.

## Mistakes I made

- I marked `try_emplace_back` `noexcept`, reading "returns `nullptr` instead of throwing" as "never throws". But `T`'s own constructor can still throw, and an exception escaping a `noexcept` function goes straight to `std::terminate`. I tested it: the program aborted with exit code 134, the caller's `catch` never ran, and the compiler gave no warning.
- I wrote `std::reinterpret_cast`. The casts are keywords, not library functions, so there's no `std::` in front of them.
- My `const` version of `slot` cast to a plain `T*` and threw the `const` away. It compiled fine until something called it on a const object, because a template's member functions are not compiled until they are used.
- `this.try_emplace_back(...)` and `this[i] = other[i]`. `this` is a pointer, and `this[i]` means the i-th *vector* in an imaginary array of them, not element `i`.
- I tried copying a `static_vector<Tracer, 3>` into a `static_vector<Tracer, 4>`. `N` is part of the type, so those are two different types with no conversion between them.
- To test the `++size_`-first bug, I moved the increment up but kept constructing into `slot(size_)`, so every element landed one slot too far. That's actually a different bug: AddressSanitizer reported a `stack-buffer-overflow` when the fourth element was written past the end of the storage.
- I checked destructor counts inside the `{ }` instead of after it, and the tests failed with `dtor=0`. Destructors run at the closing brace.

## Limitations and what I'd do next

- The `size_ == N` check on every add is what keeps the fill from being vectorised. A bulk operation that checks the capacity once (`assign(first, last)`, say) is the first thing I'd try, to see how close it gets to the array.
- `size_` is always a `std::size_t`. For small `N`, a `std::uint8_t` would make `<char, 3>` 4 bytes instead of 16.
- No `insert` or `erase` in the middle, no comparison operators, and `operator[]` doesn't check bounds (on purpose, like `std::vector`'s).
- The copy and move assignments give the basic guarantee, not the strong one: if an element's copy throws partway through, the target is left valid but only partly filled.
- I haven't dealt with `std::launder`, which the standard asks for in some cases of reading objects back out of raw storage like this. It hasn't caused a problem here, but I haven't looked into it properly yet.
- I'd like to try storing the elements in a `union` instead of bytes and see whether GCC treats the loop any differently.

## Building

Tested with g++ 12.2 on Linux, `-std=c++23`.

```bash
# tests (from this folder)
g++ -std=c++23 -Wall -Wextra -Werror -g -pthread main.cpp -o test && ./test

# sanitizers (ASan and TSan can't share a binary)
g++ -std=c++23 -Wall -Wextra -Werror -g -pthread -fsanitize=address,undefined main.cpp -o test_asan && ./test_asan
g++ -std=c++23 -Wall -Wextra -Werror -g -pthread -fsanitize=thread main.cpp -o test_tsan && ./test_tsan

# benchmark (from bench/)
g++ -std=c++23 -Wall -Wextra -Werror -O2 bench.cpp -o bench && ./bench

# the same benchmark with jumps kept off 32-byte boundaries, to check for layout effects
g++ -std=c++23 -Wall -Wextra -Werror -O2 -Wa,-mbranches-within-32B-boundaries bench.cpp -o bench && ./bench
```

## Layout

```
static_vector.h       static_vector<T, N>
main.cpp              the tests and the static_asserts
bench/bench.cpp       the benchmark, with a counting operator new and a page-fault counter
../common/tracer.h    the counting Tracer type
../common/thrower.h   Thrower, which throws on demand
../common/bench.h     the timing harness shared by every challenge
```
