// They use the classes in ../main.cpp and never change them.
// Run with ./run.sh, which puts every test in its own process so one crash can't hide the rest.
// Every test also passes against std::shared_ptr + std::weak_ptr (see the SHARED_SOURCE note in run.sh).

#ifndef SHARED_SOURCE
#define SHARED_SOURCE "../main.cpp"
#endif

#define main user_main      // the classes share a file with the user's own main()
#include SHARED_SOURCE
#undef main

#include "../../common/tracer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>

using S = Shared<Tracer>;
using W = Weak<Tracer>;

static int g_failed = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::printf("    check failed (weak_tests.cpp:%d): %s\n", __LINE__, #cond); \
            ++g_failed;                                                             \
        }                                                                           \
    } while (0)

// get() as a pointer, whichever way the class under test returns it
template <typename P>
auto* raw(const P& p) {
    if constexpr (std::is_pointer_v<decltype(p.get())>) return p.get();
    else return p ? &p.get() : nullptr;
}

// ---------------------------------------------------------------- the task's usage example

void usage_example() {
    S s(new Tracer(7));
    W w = s;
    CHECK(s.use_count() == 1);
    CHECK(!w.expired());
    {
        S t = w.lock();
        CHECK(t);
        CHECK(raw(t) == raw(s));
        CHECK(t->id == 7);
        CHECK(s.use_count() == 2);
    }
    CHECK(s.use_count() == 1);
    CHECK(Tracer::destroys == 0);
    s.reset();
    CHECK(Tracer::destroys == 1);           // destroyed on reset(), though w still watches
    CHECK(w.expired());
    S u = w.lock();
    CHECK(!u);
    CHECK(u.use_count() == 0);
    CHECK(raw(u) == nullptr);
}

// ---------------------------------------------------------------- watching is not owning

void watcher_is_not_an_owner() {
    S s(new Tracer(1));
    W w1 = s;
    W w2 = s;
    CHECK(s.use_count() == 1);
    CHECK(w1.use_count() == 1 && w2.use_count() == 1);
}

void lock_joins_the_existing_owners() {
    {
        S s(new Tracer(1));
        W w = s;
        {
            S t = w.lock();
            CHECK(s.use_count() == 2 && t.use_count() == 2);
        }
        CHECK(s.use_count() == 1);
        CHECK(Tracer::destroys == 0);       // t's going must not destroy what s still owns
    }
    CHECK(Tracer::destroys == 1);
}

void locked_owner_outlives_the_original() {
    S t;
    {
        S s(new Tracer(1));
        W w = s;
        t = w.lock();
    }
    CHECK(Tracer::destroys == 0);
    CHECK(t.use_count() == 1);
    CHECK(t && t->id == 1);
}

// ---------------------------------------------------------------- the handle it was made from changes

void handle_gone_object_gone() {
    W w;
    {
        S s(new Tracer(1));
        w = s;
    }                                       // the handle and the object are both gone
    CHECK(Tracer::destroys == 1);
    CHECK(w.expired());
    CHECK(w.use_count() == 0);
    CHECK(!w.lock());
}

void handle_gone_object_alive() {
    S keep;
    W w;
    {
        S s(new Tracer(1));
        keep = s;
        w = s;
    }                                       // s is gone; keep still owns the Tracer
    CHECK(Tracer::destroys == 0);
    CHECK(!w.expired());
    CHECK(w.use_count() == 1);
    S t = w.lock();
    CHECK(t && t->id == 1);
    CHECK(keep.use_count() == 2);
}

void handle_reset_object_alive() {
    S s(new Tracer(1));
    S keep = s;
    W w = s;
    s.reset();                              // s lets go; keep still owns the Tracer
    CHECK(!w.expired());
    CHECK(w.use_count() == 1);
    S t = w.lock();
    CHECK(t && t->id == 1);
}

void handle_reassigned() {
    S s(new Tracer(1));
    W w = s;
    S other(new Tracer(2));
    s = other;                              // s owns Tracer 2 now; Tracer 1 has no owners left
    CHECK(Tracer::destroys == 1);
    CHECK(w.expired());                     // w watched Tracer 1, not the handle s
    S t = w.lock();
    CHECK(!t);
}

// ---------------------------------------------------------------- empty, reset, copy, move

void default_constructed() {
    W w;
    CHECK(w.expired());
    CHECK(w.use_count() == 0);
    S t = w.lock();
    CHECK(!t);
    CHECK(t.use_count() == 0);
}

void reset_watches_nothing() {
    S s(new Tracer(1));
    W w = s;
    w.reset();
    CHECK(w.expired());
    CHECK(w.use_count() == 0);
    CHECK(!w.lock());
    CHECK(s.use_count() == 1);
    CHECK(Tracer::destroys == 0);
}

void copy_construct() {
    S s(new Tracer(1));
    W w1 = s;
    W w2 = w1;
    CHECK(!w1.expired() && !w2.expired());
    CHECK(s.use_count() == 1);
    s.reset();
    CHECK(w1.expired() && w2.expired());
}

void move_construct() {
    S s(new Tracer(1));
    W w1 = s;
    W w2 = std::move(w1);
    CHECK(!w2.expired());
    S t = w2.lock();
    CHECK(t && t->id == 1);
    t.reset();
    s.reset();
    CHECK(w2.expired());
}

void copy_assign() {
    S a(new Tracer(1));
    S b(new Tracer(2));
    W wa = a;
    W wb = b;
    wa = wb;
    S t = wa.lock();
    CHECK(t && t->id == 2);
    t.reset();
    b.reset();
    CHECK(wa.expired() && wb.expired());
    CHECK(Tracer::destroys == 1);           // only b's Tracer
    CHECK(a.use_count() == 1);
}

void move_assign() {
    S a(new Tracer(1));
    S b(new Tracer(2));
    W wa = a;
    W wb = b;
    wa = std::move(wb);
    S t = wa.lock();
    CHECK(t && t->id == 2);
    t.reset();
    b.reset();
    CHECK(wa.expired());
}

void self_assign() {
    S s(new Tracer(1));
    W w = s;
    w = w;
    CHECK(!w.expired());
    S t = w.lock();
    CHECK(t && t->id == 1);
    t.reset();
    w = std::move(w);                       // w's value is unspecified after this (std::weak_ptr empties it);
    CHECK(s.use_count() == 1);              // it just mustn't crash, leak, or touch the object
    CHECK(Tracer::destroys == 0);
}

// ---------------------------------------------------------------- the two lifetimes

void watchers_outlive_everything() {        // LSan checks at exit that the bookkeeping was freed
    W w1;
    W w2;
    {
        S s(new Tracer(1));
        w1 = s;
        w2 = w1;
    }
    CHECK(Tracer::destroys == 1);
    CHECK(w1.expired() && w2.expired());
    w1.reset();
    CHECK(w2.expired());
}

struct LoopNode {
    Tracer tr;
    Shared<LoopNode> next;                  // owns the node after it
    Weak<LoopNode> prev;                    // watches the node before it
    explicit LoopNode(int id) : tr(id) {}
};

void loop_with_one_weak_edge_is_freed() {
    {
        Shared<LoopNode> a(new LoopNode(1));
        Shared<LoopNode> b(new LoopNode(2));
        a->next = b;
        b->prev = a;
        CHECK(a.use_count() == 1);          // the Weak edge doesn't count
        CHECK(b.use_count() == 2);          // b, and a->next
    }
    CHECK(Tracer::destroys == 2);
}

// ---------------------------------------------------------------- the type's shape, compile-time facts

void api_shape() {
    CHECK((std::is_convertible_v<S&, W>));                   // Weak<Order> w = s; compiles
    CHECK((std::is_convertible_v<const S&, W>));             // ...from a const Shared too: watching changes nothing
    CHECK(std::is_default_constructible_v<W>);
    CHECK(std::is_copy_constructible_v<W> && std::is_copy_assignable_v<W>);
    CHECK(std::is_nothrow_move_constructible_v<W> && std::is_nothrow_move_assignable_v<W>);
    CHECK((std::is_same_v<decltype(std::declval<W&>().lock()), S>));   // lock() hands back a Shared by value
}

// ---------------------------------------------------------------- watching an empty handle (added 2026-11-11)

void watch_an_empty_shared() {
    S s;                                    // owns nothing, so there is no bookkeeping to join
    W w = s;
    CHECK(w.expired());
    CHECK(w.use_count() == 0);
    CHECK(!w.lock());
    CHECK(s.use_count() == 0);
}

void watch_a_moved_from_shared() {
    S s(new Tracer(1));
    S t = std::move(s);                     // s is empty now; t owns the Tracer
    {
        W w = s;                            // watches nothing
        CHECK(w.expired());
        CHECK(w.use_count() == 0);
        CHECK(!w.lock());
    }                                       // w's going must not touch t's bookkeeping
    CHECK(t.use_count() == 1);
    CHECK(Tracer::destroys == 0);
    W w2 = t;
    CHECK(!w2.expired());
    t.reset();
    CHECK(Tracer::destroys == 1);
    CHECK(w2.expired());
}

void copy_an_empty_weak() {
    W empty;
    W w1 = empty;                           // copy construction from a Weak that watches nothing
    CHECK(w1.expired());
    S s(new Tracer(1));
    W w2 = s;
    w2 = empty;                             // copy assignment from it: w2 stops watching s
    CHECK(w2.expired());
    CHECK(w2.use_count() == 0);
    CHECK(s.use_count() == 1);
    CHECK(Tracer::destroys == 0);
}

// ---------------------------------------------------------------- runner

struct Test {
    const char* name;
    void (*fn)();
};

static const Test g_tests[] = {
    {"the task's usage example", usage_example},
    {"a watcher is not an owner", watcher_is_not_an_owner},
    {"lock() joins the existing owners", lock_joins_the_existing_owners},
    {"a locked owner outlives the original", locked_owner_outlives_the_original},
    {"handle gone, object gone", handle_gone_object_gone},
    {"handle gone, object still owned", handle_gone_object_alive},
    {"handle reset, object still owned", handle_reset_object_alive},
    {"handle reassigned to another object", handle_reassigned},
    {"default-constructed Weak", default_constructed},
    {"reset() watches nothing", reset_watches_nothing},
    {"copy construction", copy_construct},
    {"move construction", move_construct},
    {"copy assignment", copy_assign},
    {"move assignment", move_assign},
    {"self assignment", self_assign},
    {"watchers outlive the object and every owner", watchers_outlive_everything},
    {"a loop with one Weak edge is freed", loop_with_one_weak_edge_is_freed},
    {"type shape (compile-time facts)", api_shape},
    {"watching an empty Shared", watch_an_empty_shared},
    {"watching a moved-from Shared", watch_a_moved_from_shared},
    {"copying an empty Weak", copy_an_empty_weak},
};

int main(int argc, char** argv) {
    const int n = static_cast<int>(sizeof(g_tests) / sizeof(g_tests[0]));
    if (argc < 2 || std::strcmp(argv[1], "names") == 0) {
        for (const Test& t : g_tests) std::puts(t.name);
        return 0;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);       // a crash must not lose the lines before it
    const int i = std::atoi(argv[1]);
    if (i < 0 || i >= n) return 3;
    Tracer::reset();
    g_tests[i].fn();
    return g_failed == 0 ? 0 : 2;
}
