# Unique: unique_ptr from scratch

A move-only owning smart pointer in C++20, written to find out what `std::unique_ptr` really does and what it really costs. It has a single-object form and an array form, `make_unique` with perfect forwarding, and a deleter parameter that takes up no space when the deleter is empty, so `Unique<T>` is the same 8 bytes as a raw pointer. The tests count every construction, copy, move and destruction exactly, and they run under AddressSanitizer, UBSan and ThreadSanitizer. I benchmarked it against raw `new`/`delete`, `std::unique_ptr` and `std::shared_ptr`, and read the assembly to see where the abstraction disappears and where it doesn't. It's the first of a set of small projects I'm using to get the language core solid.

```cpp
auto p   = make_unique<Tracer>(1, 2.0);    // built in place: 1 construction, no copies or moves
auto arr = make_unique<Tracer[]>(4);       // array form, freed with delete[]

struct FileCloser { void operator()(std::FILE* f) const noexcept { std::fclose(f); } };
Unique<std::FILE, FileCloser> f(std::fopen("out.txt", "w"));    // closed when f goes out of scope
```

## Results

Every variant does the same work: allocate a 224-byte node, write one field, free it. Each cycle is timed on its own with `steady_clock`, 100,000 times, built with `-O2`. One run is 5 rounds in which every variant takes a turn, and the table shows the medians. I ran it three times. The first run came out 10 to 14 ns slower across the board, which I'd seen happen on this machine before, so these numbers are from runs 2 and 3. Those two agreed to within 2 ns at p50 and 4 ns at p99.

| Variant | p50 | p99 | p99.9 |
|---|---|---|---|
| timer only (nothing between the two clock reads) | 27 ns | 29 ns | 30–34 ns |
| raw `new Node{}` + `delete` | 63–64 ns | 67 ns | 78–142 ns |
| `Unique` | 64 ns | 66–67 ns | 77–144 ns |
| `std::unique_ptr` | 64 ns | 66–67 ns | 80–86 ns |
| `std::make_shared` | 64–66 ns | 66–70 ns | 69–72 ns |
| `std::shared_ptr(new Node())` | 84–85 ns | 89–90 ns | 160–222 ns |
| raw `new Node` (not zeroed) | 46–47 ns | 49–50 ns | 52–67 ns |

`Unique` costs nothing over raw `new`/`delete`, and you can't tell it apart from `std::unique_ptr`. Every row includes the 27 ns it takes to read the clock twice, so the actual allocate, zero and free comes to about 37 ns.

The biggest piece of those 37 ns isn't the allocator. Zeroing the 224 bytes takes about 17 ns, which you can see in the last row. Every `make_*` function zero-initialises, so the raw version has to use `new Node{}` for the comparison to be fair. With plain `new Node` it would look 17 ns quicker than the smart pointers for a reason that has nothing to do with them. I'd seen this before: skipping the zeroing saved about 16 ns per message in my logger.

`make_shared` came out as cheap as `unique_ptr`. It puts the object and its reference counts in one allocation, and when both counts are 1 (one owner, no weak pointers), libstdc++ 12 releases it with a single atomic load instead of two atomic decrements. So a `shared_ptr` that's never copied pays almost nothing for being shared. `shared_ptr(new T)` is about 20 ns slower because the counts get their own allocation: I hooked `operator new` and counted two calls instead of one. What `shared_ptr` is known to cost, atomic counts going back and forth between threads, doesn't happen in this benchmark at all.

p50 and p99 came out the same in both runs, and p99.9 didn't, so I'd only lean on the first two. The machine is a Chromebook: an Intel Pentium 6405U (2 cores, 4 threads) running Linux in a KVM virtual machine, with g++ 12.2.

## What the assembly showed

At `-O2`, a function that does `new int(v)`, reads the value and deletes it compiles to two instructions, `lea eax,[rdi+0x1]` and `ret`. The same function written with `Unique` compiles to the same two. GCC is allowed to remove an allocation whose result never escapes, so the `new` and `delete` are simply gone, and `Unique` adds nothing on top.

Passing one by value is the exception. Under the System V calling convention, a class with a non-trivial destructor can't be passed in a register, so the caller puts it in memory and passes its address, and the function pays one extra load before it can use the pointer. `std::unique_ptr` does exactly the same. That's the one place "zero overhead" isn't quite true, and it's why a function that only borrows the object should take `T*` or `T&` instead.

## How it works

**Moving.** Copying is deleted. Moving copies the 8-byte pointer and sets the source to null, and the object itself never moves: after moving a `Unique<Tracer>`, the Tracer is at the same address and none of its constructors have run.

**The deleter is a template parameter.** `Unique<T, D = DefaultDelete<T>>` stores its `D` as a member marked `[[no_unique_address]]`. Without the attribute, even an empty deleter takes a byte (every member needs its own address) and padding rounds that up to 8, making `Unique` 16 bytes. With it, `Unique<T>` stays at 8, and a `static_assert` holds it there. A deleter with real state costs exactly that state: one holding an `int` makes it 16. For comparison, `std::unique_ptr` with a function-pointer deleter is also 16 bytes, and calls through the pointer on every release instead of calling `fclose` directly.

**A deleter never sees `nullptr`.** `delete nullptr` is harmless, but a custom deleter might not be. An empty `Unique` holding an `fclose` deleter segfaulted until every release path checked for null first.

**New pointer first, then delete the old one.** `reset()` and move assignment store the new pointer before deleting the old object. Deleting first breaks if the old object's destructor reaches back into the same `Unique`. I built an object that owns the `Unique` that owns it, and the delete-first version overflowed the stack. The safer order also compiled to less code: 8 instructions ending in a tail call, against 15 with a stack frame, because once the store is done nothing has to survive the call to `operator delete`.

**Arrays get their own specialisation.** `Unique<T[]>` replaces `*` and `->` with `operator[]` and frees with `delete[]`. For a type with a destructor, `new T[n]` quietly stores the element count in the 8 bytes in front of the array (`new Tracer[4]` asks for 72 bytes, not 64), and plain `delete` hands `free()` the wrong address.

**`make_unique` forwards.** Its arguments go through `std::forward`, so an lvalue argument gets copied and an rvalue gets moved. With `std::move` there instead, the forwarding test showed `copy=0 move=2`: the factory was emptying out a variable the caller still owned.

## Testing

Everything builds with `-Wall -Wextra -Werror`, in three builds: a normal one, AddressSanitizer + UBSan, and ThreadSanitizer. `main.cpp` has 17 test cases and 7 `static_assert`s (not copyable, `noexcept` move, exactly pointer-sized, for both forms).

The tests use a `Tracer` type that counts its constructions, copies, moves, assignments and destructions, so each one checks exact numbers instead of just "it didn't crash". Building through `make_unique<Tracer>(t)` has to show exactly one copy. Moving a `Unique` has to show zero Tracer moves. A counting deleter checks that releasing goes through the deleter on all three paths (destructor, `reset()`, move assignment) and never happens for an empty pointer.

I broke the code on purpose to check that the tests notice: taking out the self-move check, leaving a plain `delete` in `reset()`, dropping the null check, and `delete` in place of `delete[]` all fail a test or a sanitizer. The `reset()` case is the interesting one. A plain `delete` still destroys the Tracer, so the construction and destruction counts stay perfect, and only counting the deleter's own calls catches it.

## Mistakes I made

- My first move constructor took a `const Unique&&` and called `std::move` on the raw pointer. It compiled, both `static_assert`s passed, and then it freed the same object twice (`free(): double free detected in tcache 2`). The `const` stops you nulling the source, and `std::move` on a pointer is just a copy, so both objects owned the Tracer. The `static_assert`s only check that a `noexcept` move constructor exists, not that it does the right thing.
- My first move assignment destroyed the object on self-move (`u = std::move(u)`) and left `u` empty. Nothing crashed and ASan didn't say anything. Only the destruction counter showed it.
- `delete` instead of `delete[]` was silent for an `int` array and a segfault for a `Tracer` array. ASan said why: the pointer was "8 bytes inside of 72-byte region", past the hidden count.
- Later I put `delete[]` in the single-object deleter, which is the same bug the other way round. ASan caught it reading "8 bytes to the left of 16-byte region".
- `Unique a(Node());` declares a function called `a`, not an object. It's C++'s "most vexing parse", and GCC has a warning named after it.
- I once put `delete this` in a destructor. It doesn't delete twice. `delete` calls the destructor, which calls `delete`, and so on until the stack runs out.

## Limitations and what I'd do next

- There's no constructor that takes a deleter object, so a deleter with state (one that returns objects to a particular pool, say) can't be given its state, and the deleter isn't moved along with the pointer. `std::unique_ptr` does both.
- The array version only works with the default deleter. `Unique<T[], D>` isn't supported.
- No converting constructor (`Unique<Derived>` to `Unique<Base>`) and no comparison operators.
- The benchmark is single-threaded, so it never touches the thing `shared_ptr` is known for, its reference counts bouncing between cores. That's the next project, a `shared_ptr` of my own.

## Building

Tested with g++ 12.2 on Linux, `-std=c++20`.

```bash
# tests (from this folder)
g++ -std=c++20 -Wall -Wextra -Werror -g -pthread main.cpp -o test && ./test

# sanitizers (ASan and TSan can't share a binary)
g++ -std=c++20 -Wall -Wextra -Werror -g -pthread -fsanitize=address,undefined main.cpp -o test_asan && ./test_asan
g++ -std=c++20 -Wall -Wextra -Werror -g -pthread -fsanitize=thread main.cpp -o test_tsan && ./test_tsan

# benchmark (from bench/)
g++ -std=c++20 -Wall -Wextra -Werror -O2 bench.cpp -o bench && ./bench

# assembly (from asm/)
g++ -std=c++20 -O2 -c zero.cpp -o zero.o && objdump -d -C --no-show-raw-insn -M intel zero.o
```

## Layout

```
Unique.h             Unique<T, D>, Unique<T[]>, DefaultDelete, and both make_unique overloads
main.cpp             the 17 test cases and the static_asserts
../common/tracer.h   the counting Tracer type the tests use
bench/bench.cpp      the benchmark
asm/zero.cpp         the functions I read in assembly
```
