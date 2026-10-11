#include <cstdio>
#include <type_traits>
#include <utility>

#include "Unique.h"
#include "../common/tracer.h"



// ---- compile-time checks: these cost nothing at runtime ----
// static_assert(condition, "message")  --> if condition is false, compile error with "message" shown
static_assert(!std::is_copy_constructible_v<Unique<int>>,        "Unique must not be copyable");
static_assert(!std::is_copy_assignable_v<Unique<int>>,           "Unique must not be copy-assignable");
static_assert(std::is_nothrow_move_constructible_v<Unique<int>>, "move constructor must be noexcept");
static_assert(sizeof(Unique<int>) == sizeof(int*),               "Unique must be pointer-sized");

static_assert(!std::is_copy_constructible_v<Unique<int[]>>,        "array Unique must not be copyable");
static_assert(std::is_nothrow_move_constructible_v<Unique<int[]>>, "array move constructor must be noexcept");
static_assert(sizeof(Unique<int[]>) == sizeof(int*),               "array Unique must be pointer-sized");



// A deleter that counts how often it is called (used by test 16)
// Btw the counter is static, so the struct still has no data members
struct CountingDelete {
    static inline int calls = 0;
    void operator()(Tracer* p) const noexcept { 
        ++calls;
        delete p; 
    }
};

// A deleter with real state (one int), used by test 17 to see what state costs in size.
struct TaggedDelete {
    int tag = 0;
    void operator()(Tracer* p) const noexcept { delete p; }
};



int main() {
    int failures = 0;
    auto fail = [&failures](const char* msg) { std::printf("       -> %s\n", msg); ++failures; };


    // 1. construct, observe, destroy
    Tracer::reset();
    {
        Unique<Tracer> u(new Tracer(1, 2.0));
        if (u.get() == nullptr) fail("u should own a Tracer");
        if (!u)                 fail("operator bool should be true when owning");
        if ((*u).id != 1)       fail("operator* gave the wrong id");
        if (u->id != 1)         fail("operator-> gave the wrong id");
    }
    if (!Tracer::expect(1, 0, 0, 0, 0, 1, "construct + destroy")) ++failures;


    // 2. moving a Unique moves the POINTER, never the Tracer
    Tracer::reset();
    {
        Unique<Tracer> a(new Tracer);
        Tracer* owned = a.get();
        Unique<Tracer> b(std::move(a));
        if (a.get() != nullptr) fail("moved-from Unique must be empty");
        if (b.get() != owned)   fail("b should hold exactly the pointer a had");
        if (a)                  fail("moved-from Unique must be false");
    }
    if (!Tracer::expect(1, 0, 0, 0, 0, 1, "move construct")) ++failures;


    // 3. release gives up ownership WITHOUT deleting
    Tracer::reset();
    {
        Unique<Tracer> u(new Tracer);
        Tracer* raw = u.release();
        if (u.get() != nullptr)    fail("release must leave the Unique empty");
        if (Tracer::destroys != 0) fail("release must not delete anything");
        delete raw;                            // we own it now
        if (Tracer::destroys != 1) fail("manual delete should have destroyed it");
    }
    if (!Tracer::expect(1, 0, 0, 0, 0, 1, "release + manual delete")) ++failures;


    // 4. reset deletes the old object and adopts the new one
    Tracer::reset();
    {
        Unique<Tracer> u(new Tracer(7, 0.0));
        u.reset(new Tracer(8, 0.0));
        if (Tracer::destroys != 1) fail("reset must delete the old object");
        if (u->id != 8)            fail("u should now own the new Tracer");
        u.reset();
        if (u.get() != nullptr)    fail("reset() with no argument must empty it");
        if (Tracer::destroys != 2) fail("reset() should have deleted the second one");
    }
    if (!Tracer::expect(2, 0, 0, 0, 0, 2, "reset")) ++failures;


    // 5. move assignment deletes whatever the target already owned
    Tracer::reset();
    {
        Unique<Tracer> a(new Tracer(1, 0.0));
        Unique<Tracer> b(new Tracer(2, 0.0));
        a = std::move(b);
        if (Tracer::destroys != 1) fail("a's original object should be deleted");
        if (b.get() != nullptr)    fail("b must be empty after being moved from");
        if (a->id != 2)            fail("a should own what b had");
    }
    if (!Tracer::expect(2, 0, 0, 0, 0, 2, "move assign")) ++failures;


    // 6. self-move must not destroy anything
    Tracer::reset();
    {
        Unique<Tracer> u(new Tracer(5, 0.0));
        Tracer* before = u.get();
        u = std::move(u);
        if (Tracer::destroys != 0) fail("self-move must not delete the object");
        if (u.get() != before)     fail("self-move must leave u owning the same object");
    }
    if (!Tracer::expect(1, 0, 0, 0, 0, 1, "self-move")) ++failures;


    // 7. swap exchanges ownership and deletes nothing
    Tracer::reset();
    {
        Unique<Tracer> a(new Tracer(1, 0.0));
        Unique<Tracer> b(new Tracer(2, 0.0));
        Tracer* pa = a.get();
        Tracer* pb = b.get();
        a.swap(b);
        if (a.get() != pb || b.get() != pa) fail("swap must exchange the two pointers");
        if (Tracer::destroys != 0)          fail("swap must not delete anything");
    }
    if (!Tracer::expect(2, 0, 0, 0, 0, 2, "swap")) ++failures;


    // 8. an empty Unique is safe everywhere it is allowed to be
    Tracer::reset();
    {
        Unique<Tracer> u;
        if (u)                      fail("a default-constructed Unique must be false");
        if (u.get() != nullptr)     fail("a default-constructed Unique must hold nullptr");
        u.reset();                             // no-op
        if (u.release() != nullptr) fail("release on an empty Unique returns nullptr");
    }
    if (!Tracer::expect(0, 0, 0, 0, 0, 0, "empty Unique")) ++failures;

    
    // 9. const Unique fixes the POINTER, not the object it points at
    Tracer::reset();
    {
        const Unique<Tracer> cu(new Tracer(3, 0.0));
        (*cu).id = 9;                          // must compile: T& not const T&
        if (cu->id != 9) fail("should be able to modify the object through a const Unique");
    }
    if (!Tracer::expect(1, 0, 0, 0, 0, 1, "const Unique")) ++failures;


    // 10. make_unique forwards: lvalue -> copy, rvalue -> move, args -> in place
    Tracer::reset();
    {
        Tracer t(7, 1.5);
        auto u = make_unique<Tracer>(t);
        if (t.id != 7)  fail("make_unique must COPY from an lvalue, not steal from it");
        auto v = make_unique<Tracer>(std::move(t));
        auto w = make_unique<Tracer>(3, 4.0);
        if (u->id != 7) fail("u should hold a copy of t");
        if (v->id != 7) fail("v should hold what was moved out of t");
        if (w->id != 3) fail("w should be built in place from (3, 4.0)");
    }
    if (!Tracer::expect(2, 1, 1, 0, 0, 4, "make_unique forwarding")) ++failures;


    // 11. make_unique<int[]> value-initialises: every element is zero
    {
        auto a = make_unique<int[]>(4);
        if (a[0] != 0 || a[1] != 0 || a[2] != 0 || a[3] != 0) fail("make_unique<int[]>(4) must give four zeros");
    }


    // 12. make_unique<Tracer[]> builds and destroys every element exactly once
    Tracer::reset();
    {
        auto t = make_unique<Tracer[]>(4);
        if (!t) fail("array Unique should be true when owning");
    }
    if (!Tracer::expect(4, 0, 0, 0, 0, 4, "make_unique<Tracer[]>(4)")) ++failures;


    // 13. operator[] returns a reference: writes reach the owned element
    {
        auto t = make_unique<Tracer[]>(4);
        t[2].id = 42;
        if (t[2].id != 42) fail("t[2].id = 42 should change the element itself");
        if (t[1].id != 0)  fail("the other elements should be untouched");
    }


    // 14. array moves and reset: every delete in Unique<T[]> must be delete[]
    Tracer::reset();
    {
        auto a = make_unique<Tracer[]>(4);
        Unique<Tracer[]> b(std::move(a));
        if (a.get() != nullptr) fail("moved-from array Unique must be empty");
        auto c = make_unique<Tracer[]>(2);
        c = std::move(b);
        if (Tracer::destroys != 2) fail("move assign must destroy the target's old array");
        c.reset(new Tracer[3]);
        if (Tracer::destroys != 6) fail("reset must destroy the old array");
    }
    if (!Tracer::expect(9, 0, 0, 0, 0, 9, "array moves + reset")) ++failures;


    // 15. array swap and release
    Tracer::reset();
    {
        auto a = make_unique<Tracer[]>(2);
        auto b = make_unique<Tracer[]>(3);
        Tracer* first = a.get();
        a.swap(b);
        if (b.get() != first) fail("swap must exchange the two arrays");
        Tracer* raw = a.release();
        if (a) fail("release must leave the array Unique empty");
        delete[] raw;                          // we own it now: delete[], not delete
    }
    if (!Tracer::expect(5, 0, 0, 0, 0, 5, "array swap + release")) ++failures;

    
    // 16. a custom deleter is used on every release path, and never handed nullptr
    Tracer::reset();
    CountingDelete::calls = 0;
    {
        Unique<Tracer, CountingDelete> a(new Tracer(1, 0.0));
        a.reset(new Tracer(2, 0.0));                    // release path 1: reset()
        if (CountingDelete::calls != 1) fail("reset() must release through the deleter");

        Unique<Tracer, CountingDelete> b(new Tracer(3, 0.0));
        a = std::move(b);                               // release path 2: move assignment
        if (CountingDelete::calls != 2) fail("move assignment must release through the deleter");

        Unique<Tracer, CountingDelete> empty;           // owns nothing
    }   // release path 3: a's destructor. b (moved from) and empty own nothing: no call for them
    if (CountingDelete::calls != 3) fail("the destructor must release through the deleter, and never call it with nullptr");
    if (!Tracer::expect(3, 0, 0, 0, 0, 3, "custom deleter")) ++failures;


    // 17. the "sizeof" test
    Tracer::reset();
    {
        Unique<Tracer> a(new Tracer(1, 0.0));
        std::printf("size of Unique<Tracer> with empty DefaultDelete: %zu\n", sizeof(a));

        Unique<Tracer, TaggedDelete> b(new Tracer(2, 0.0));
        std::printf("size of Unique<Tracer> with a deleter holding an int: %zu\n", sizeof(b));

    }



    std::printf("\n%s (%d failure%s)\n",
                failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

