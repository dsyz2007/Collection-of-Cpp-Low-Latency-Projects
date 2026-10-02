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

    std::printf("\n%s (%d failure%s)\n",
                failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

