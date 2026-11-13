# Shared and Weak: my own shared_ptr and weak_ptr

A reference-counted owning pointer `Shared<T>` and a non-owning `Weak<T>` in C++23, written from an empty file. "Implement `shared_ptr`" is a common interview question because one small class covers ownership, object lifetimes, exceptions, threads and cost, so I did it as a mock interview. I wrote the code and got pushed on it round after round. The part that took me longest was `Weak`, and I only really understood why libstdc++ starts its weak count at 1 (instead of 0) after my own version broke without it.

```cpp
struct Order { int id; };

Shared<Order> s(new Order{7});
Weak<Order> w = s;                 // watches the Order, doesn't own it
s.use_count();                     // 1

{
    Shared<Order> t = w.lock();    // the Order is alive, so t becomes a second owner
    s.use_count();                 // 2
}

s.reset();                         // last owner lets go, the Order is deleted here
w.expired();                       // true
Shared<Order> u = w.lock();        // empty
```

## How it works

Every handle, `Shared` or `Weak`, holds three pointers: the object, a strong count and a watchers count. The two counts live on the heap so every copy sees the same numbers.

```
Shared s --+
Shared t --+--> Order
Weak   w --+    count    = 2   (one per Shared)
                watchers = 2   (one per Weak, plus 1 for all the owners together)
```

**Two counts because there are two lifetimes.** The object dies when the strong count gets to 0. But a `Weak` still has to be able to ask "is it alive?" after that, and it can't ask the object, and it can't check its own copy of the pointer either, because `delete` leaves the old address in the pointer. Only the count knows. So the counts have to outlive the object, and something has to decide when the counts themselves can go. That's the watchers count. When it gets to 0, both counts get deleted.

**The owners count as one watcher.** Deleting the object runs its destructor, and that destructor can destroy a `Weak` that lives inside the object (a back-pointer in a graph, for example). Before I did this, that `Weak` saw both counts at 0 and freed them while the last owner was still in the middle of its own release code, and the owner then read freed memory. My loop test (two nodes, one `Weak` back edge) showed it as a heap-use-after-free. Now `watchers` starts at 1, the last owner gives that 1 back only after the object is completely destroyed, and whoever brings it to 0 frees both counters. libstdc++ starts both of its counts at 1 for the same reason.

**`lock()` joins the existing owners.** If the strong count is 0 it returns an empty `Shared`. Otherwise it builds a `Shared` with the same three pointers and adds one to the count, which is basically what the copy constructor does. My first version went through `Shared(T*)`, which starts a brand new count, so the object ended up with two counts and got deleted twice.

**Assignment is copy-and-swap.** Make a temporary copy (that's where the one `++` happens), swap the pointer members with it, and the temporary's destructor lets go of the old object at the closing brace. That keeps `x = x` safe, and also `head = head->next`, where `other` lives inside the node that's being let go. Letting go first would delete `other` before reading it. `Weak` does the same thing with the watchers count.

**Moves leave the source empty.** All three pointers get nulled, so a moved-from handle holds nothing and its destructor does nothing. `std::move` on a raw pointer is just a copy, so this has to be done by hand.

## What it costs

| | mine | std |
|---|---|---|
| `sizeof` a `Shared` | 24 bytes | 16 |
| `sizeof` a `Weak` | 24 bytes | 16 |
| heap allocations to make one object | 3 (object, count, watchers) | 2 for `shared_ptr(new T)`, 1 for `make_shared` |

Copies, `Weak`s and `lock()` don't allocate at all. The counts are plain `int`s, so a copy is one ordinary `add`. That's also why it isn't thread-safe (see below). I haven't benchmarked the copy cost yet.

## Testing

The tests are in `tests/`. The challenge ran as a mock interview where I wrote the classes, and the mock interviewer side (Claude) wrote the tests and constantly probed me on any potential shortcomings in my code and efficiency of my system designs using its tests. There are 15 tests for `Shared` and 21 for `Weak`. `run.sh` runs each test in its own process so one crash can't hide the rest, and every test also passes against `std::shared_ptr` and `std::weak_ptr`, so the tests themselves are checked.

Three builds, all with `-Wall -Wextra -Werror`: a normal one, AddressSanitizer + UBSan, and ThreadSanitizer. It passes 15 of 15 and 21 of 21 in all three. Only the ASan build checks for leaks. At one point `Shared` passed 15 of 15 in the normal and TSan builds and 2 of 15 under ASan, because I was never freeing the watchers counter.

The `Weak` tests cover:
- the usage example above
- a `Weak` not counting as an owner
- `lock()` joining the owners, and a locked `Shared` outliving the original
- the `Shared` a `Weak` was made from going out of scope, being reset or being reassigned while the object is still owned by someone else
- `Weak`s made from empty and moved-from `Shared`s, and copying an empty `Weak`
- copy, move, copy assignment, move assignment and self assignment
- `Weak`s that outlive the object and every owner (the leak check catches it if the counts are never freed)
- a two-node loop with one `Weak` edge being freed
- compile-time checks: a `Weak` can be made from a `const Shared`, moves are `noexcept`, `lock()` returns by value

## Mistakes I made

- I thought `std::move` on a raw pointer left garbage behind. It copies it, so both handles had the object and the count, and the second one used it after it was deleted.
- I thought `delete ptr` sets `ptr` to null. It doesn't, so `if (a)` said true about a dead object.
- My first `Weak` held a pointer to the `Shared` it was made from. That's watching a variable, not the object. When that variable went out of scope it read a dead stack variable.
- `lock()` returned `Shared<T>&&` to a temporary. GCC actually returns a null reference there, so my `main` segfaulted at address 0.
- `std::swap(*this, temp)` inside my own move assignment. `std::swap` uses my move assignment, so it recursed until the stack overflowed.
- My `Weak` copy and move first just copied pointers. Since every `Weak` gives back one watcher when it dies, the count went wrong in both directions, too low (freed early) or never reaching 0 (leaked), depending on which handle died first.
- `lock()` checked the `Weak`'s own copy of the object pointer to see if it was alive. Nothing updates that copy when the object is deleted.

## Limitations and what I'd do next

- **Not thread-safe.** The counts are plain `int`s. With two threads copying and dropping one handle, an unoptimised build crashed or ended with the wrong count in all 8 runs (4 aborts, 1 segfault, 3 wrong counts). At `-O2` it looked fine, but only because GCC removed the `++`/`--` pair, and TSan reported the race. Next step: `std::atomic<int>`, relaxed on the increment, `acq_rel` on the decrement, and a compare-exchange loop in `lock()` so it can never go from 0 back to 1.
- **`Shared(T*)` leaks if an allocation throws.** If `new int` throws, the object passed in is never deleted (4 bytes leaked in my test), or the object and the count if it's the second one (8 bytes). `std::shared_ptr(new T)` deletes it, and didn't leak in the same test.
- **Internals are public.** The constructor `lock()` uses and two accessors that hand out the count pointers are public, so anyone can break the counts. They should be private with `Weak` as a friend.
- **Three allocations and 24-byte handles.** Putting both counts in one struct would make it two allocations and 16-byte handles, and putting the object in the same block like `make_shared` would make it one.
- Two `Shared`s made from the same raw pointer get separate counts and delete it twice. `std::shared_ptr` has the same problem.
- No custom deleters, arrays, `make_shared`, aliasing constructor or `enable_shared_from_this`.

## Building

Tested with g++ 12.2 on Linux, `-std=c++23`.

```bash
# the usage example in main.cpp
g++ -std=c++23 -Wall -Wextra -Werror -g -pthread main.cpp -o Shared && ./Shared

# tests: mode is asan (the default), plain or tsan, suite is all (the default), shared or weak
cd tests && ./run.sh asan all
```

## Layout

```
main.cpp               Shared<T>, Weak<T> and the usage example
tests/tests.cpp        15 tests for Shared class
tests/weak_tests.cpp   21 tests for Weak class
tests/run.sh           builds a suite and runs each test in its own process
../common/tracer.h     Tracer, which counts constructions and destructions for the tests
```
