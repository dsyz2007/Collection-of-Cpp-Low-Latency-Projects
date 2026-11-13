// They use the class in ../main.cpp and never change it.
// Run with ./run.sh, which puts every test in its own process so one crash can't hide the rest.
// Every test also passes against std::shared_ptr (see the SHARED_SOURCE note in run.sh).

#ifndef SHARED_SOURCE
#define SHARED_SOURCE "../main.cpp"
#endif

#define main user_main      // the class shares a file with the user's own main()
#include SHARED_SOURCE
#undef main

#include "../../common/tracer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

using S = Shared<Tracer>;

static int g_failed = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::printf("    check failed (tests.cpp:%d): %s\n", __LINE__, #cond);  \
            ++g_failed;                                                             \
        }                                                                           \
    } while (0)

// get() as a pointer, whichever way the class under test returns it
template <typename P>
Tracer* raw(const P& p) {
    if constexpr (std::is_pointer_v<decltype(p.get())>) return p.get();
    else return p ? &p.get() : nullptr;
}

// ---------------------------------------------------------------- the task's usage example

void usage_example() {
    S a(new Tracer(1));
    S b = a;
    CHECK(a.use_count() == 2);
    CHECK(b->id == 1);
    CHECK((*b).id == 1);
    a.reset();
    CHECK(!a);
    CHECK(a.use_count() == 0);
    CHECK(Tracer::destroys == 0);
    CHECK(b.use_count() == 1);
    S c = std::move(b);
    CHECK(!b);
    CHECK(b.use_count() == 0);
    CHECK(c.use_count() == 1);
    CHECK(c);
    c.reset();
    CHECK(Tracer::destroys == 1);
    CHECK(Tracer::copies == 0 && Tracer::moves == 0);
}

// ---------------------------------------------------------------- owners coming and going

void last_owner_destroys() {
    {
        S a(new Tracer(1));
        CHECK(a.use_count() == 1);
        CHECK(Tracer::destroys == 0);
    }
    CHECK(Tracer::destroys == 1);
}

void copy_shares() {
    {
        S a(new Tracer(1));
        {
            S b = a;
            CHECK(a.use_count() == 2 && b.use_count() == 2);
            CHECK(raw(a) == raw(b));
        }
        CHECK(a.use_count() == 1);
        CHECK(Tracer::destroys == 0);
    }
    CHECK(Tracer::destroys == 1);
    CHECK(Tracer::copies == 0);
}

void move_transfers() {
    {
        S a(new Tracer(1));
        S b = std::move(a);
        CHECK(!a);
        CHECK(b.use_count() == 1);
        CHECK(Tracer::moves == 0);
    }
    CHECK(Tracer::destroys == 1);
}

void moved_from_reports_empty() {
    S a(new Tracer(1));
    S b = std::move(a);
    CHECK(a.use_count() == 0);
    CHECK(raw(a) == nullptr);
}

void moved_into_vector_outlives_scope() {
    std::vector<S> book;
    {
        S s(new Tracer(1));
        book.push_back(std::move(s));
    }
    CHECK(Tracer::destroys == 0);
    CHECK(book[0]->id == 1);
}

// ---------------------------------------------------------------- reset

void reset_one_of_two_owners() {
    S b(new Tracer(1));
    {
        S a = b;
        a.reset();
        CHECK(!a);
        CHECK(a.use_count() == 0);
        CHECK(b.use_count() == 1);
    }
    CHECK(Tracer::destroys == 0);
    CHECK(b.use_count() == 1);
    CHECK(b->id == 1);
}

void reset_last_owner() {
    {
        S a(new Tracer(1));
        a.reset();
        CHECK(Tracer::destroys == 1);
        CHECK(!a);
        CHECK(a.use_count() == 0);
    }
    CHECK(Tracer::destroys == 1);
}

void reset_twice() {
    S a(new Tracer(1));
    a.reset();
    a.reset();
    CHECK(Tracer::destroys == 1);
}

// ---------------------------------------------------------------- assignment

void copy_assign() {
    {
        S x(new Tracer(1));
        S y(new Tracer(2));
        x = y;
        CHECK(Tracer::destroys == 1);       // x's old Tracer goes straight away
        CHECK(x.use_count() == 2 && y.use_count() == 2);
        CHECK(x->id == 2);
    }
    CHECK(Tracer::destroys == 2);
}

void move_assign() {
    {
        S x(new Tracer(1));
        S y(new Tracer(2));
        x = std::move(y);
        CHECK(Tracer::destroys == 1);       // x's old Tracer goes straight away
        CHECK(!y);
        CHECK(x.use_count() == 1);
        CHECK(x->id == 2);
    }
    CHECK(Tracer::destroys == 2);
}

void self_assign() {
    S x(new Tracer(1));
    x = x;
    CHECK(Tracer::destroys == 0);
    CHECK(x.use_count() == 1);
    x = std::move(x);
    CHECK(Tracer::destroys == 0);
    CHECK(x.use_count() == 1);
    CHECK(x->id == 1);
}

// ---------------------------------------------------------------- empty handles

void copy_and_assign_empty() {
    S a(new Tracer(1));
    S b = std::move(a);                     // a is empty now, with no default constructor needed
    {
        S c = a;
        CHECK(!c);
        CHECK(c.use_count() == 0);
    }
    S d(new Tracer(2));
    d = a;                                  // assigning empty over an owner lets go of its object
    CHECK(Tracer::destroys == 1);
    CHECK(!d);
    a = b;                                  // assigning onto an empty handle
    CHECK(a.use_count() == 2);
}

template <typename P>
void default_constructed() {
    if constexpr (std::is_default_constructible_v<P>) {
        P e;
        CHECK(!e);
        CHECK(e.use_count() == 0);
        CHECK(raw(e) == nullptr);
    } else {
        std::printf("    no default constructor\n");
        ++g_failed;
    }
}

// ---------------------------------------------------------------- the type's shape, compile-time facts

void api_shape() {
    CHECK(std::is_default_constructible_v<S>);
    CHECK((std::is_copy_constructible_v<S>));        // can copy from a const handle
    CHECK((std::is_copy_assignable_v<S>));           // can assign from a const handle
    CHECK((std::is_pointer_v<decltype(std::declval<const S&>().get())>));   // an empty handle's get() must be able to say null
    CHECK((!std::is_convertible_v<Tracer*, S>));     // Shared<T> s = new T; must not compile
    CHECK((std::is_nothrow_move_constructible_v<S>));
    CHECK((std::is_nothrow_move_assignable_v<S>));
}

// ---------------------------------------------------------------- runner

struct Test {
    const char* name;
    void (*fn)();
};

static const Test g_tests[] = {
    {"the task's usage example", usage_example},
    {"last owner destroys", last_owner_destroys},
    {"copy shares", copy_shares},
    {"move transfers", move_transfers},
    {"moved-from handle reports empty", moved_from_reports_empty},
    {"moved into a vector, outlives its scope", moved_into_vector_outlives_scope},
    {"reset one of two owners", reset_one_of_two_owners},
    {"reset the last owner", reset_last_owner},
    {"reset twice", reset_twice},
    {"copy assignment", copy_assign},
    {"move assignment", move_assign},
    {"self assignment", self_assign},
    {"copying and assigning an empty handle", copy_and_assign_empty},
    {"default-constructed handle", default_constructed<S>},
    {"type shape (compile-time facts)", api_shape},
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
