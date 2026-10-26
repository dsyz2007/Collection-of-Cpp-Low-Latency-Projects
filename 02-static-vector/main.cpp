#include "static_vector.h"
#include "../common/tracer.h"
#include "../common/thrower.h"

#include <cstdio>    // for std::printf()
#include <cstddef>   
#include <cassert>   // for assert()
#include <utility>   // for std::move()


// compile-time static asserts
static_assert(std::is_nothrow_move_constructible_v<static_vector<Tracer, 4>>);
static_assert(!std::is_nothrow_move_constructible_v<static_vector<Thrower, 4>>);


int main() {

    std::printf("%zu\n", sizeof(static_vector<int, 8>));

    std::printf("%zu\n", sizeof(static_vector<char, 3>));

    std::printf("%zu\n", sizeof(static_vector<Tracer, 4>));
  

    // Test 1
    Tracer::reset();
    {
        static_vector<Tracer, 4> v;

        Tracer* p1 = v.try_emplace_back(0, 0.0);
        Tracer* p2 = v.try_emplace_back(1, 0.0);
        v.try_emplace_back(2, 0.0);
        v.try_emplace_back(3, 0.0);
        Tracer* p5 = v.try_emplace_back(4, 0.0);

        assert(p5 == nullptr);
        assert((char*)p2 - (char*)p1 == sizeof(Tracer));
        std::printf("%d\n", v.full());
        
        Tracer::expect(4,0,0,0,0,0,"fill to capacity");
    }


    // Test 2
    Tracer::reset();
    {
        static_vector<Tracer, 8> v;

        for (int i = 0; i < 3; ++i) {
            v.try_emplace_back(i, 0.0);
        }

        assert(v.size() == 3);
    }
    Tracer::expect(3,0,0,0,0,3,"Emplace back 3 times");
    

    // Test 3
    Tracer::reset();
    {
        static_vector<Tracer, 2> v;

        Tracer t(7, 1.0);
        v.try_emplace_back(t);
        v.try_emplace_back(std::move(t));
    }
    Tracer::expect(1,1,1,0,0,3,"Emplace back a moved tracer");


    // Test 4
    Tracer::reset();
    {
        static_vector<Tracer, 2> v;

        v.try_emplace_back(0, 0.0);
        v.try_emplace_back(1, 0.0);
        Tracer* ptr = v.try_emplace_back(2, 0.0);

        assert(ptr == nullptr);
    }
    Tracer::expect(2,0,0,0,0,2,"Overflow and check for nullptr");


    // Test 5
    Tracer::reset();
    {
        static_vector<Tracer, 4> v;

        v.try_emplace_back(0, 0.0);
        v.try_emplace_back(1, 0.0);
        v.pop_back();

        assert(v.size() == 1);

        Tracer::expect(2,0,0,0,0,1,"Pop back");
    }


    // Test 6
    Tracer::reset();
    {
        static_vector<Tracer, 10> v;

        v.try_emplace_back(0, 0.0);
        v.try_emplace_back(1, 0.0);
        v.try_emplace_back(2, 0.0);
        v.clear();

        assert(v.empty());

        Tracer::expect(3,0,0,0,0,3,"Clear");
    }


    // Test 7
    Tracer::reset();
    {
        static_vector<Tracer, 10> v;

        v.try_emplace_back(0, 0.0);
        v.try_emplace_back(1, 0.0);
        v.try_emplace_back(2, 0.0);
        v[1].id = 42;

        assert(v[1].id == 42);

        std::printf("Test 7: v[1].id = %d  %s\n", v[1].id, v[1].id == 42 ? "[PASS]" : "[FAIL]");
    }


    // Test 8
    Tracer::reset();
    {
        static_vector<Tracer, 10> v;

        v.try_emplace_back(0, 0.0);
        v.try_emplace_back(1, 0.0);
        v.try_emplace_back(2, 0.0);
        
        const auto& cv = v;

        int count_cv = 0, count_v = 0;

        for ([[maybe_unused]] const auto& x : cv) {
            ++count_cv;
        }

        for ([[maybe_unused]] auto& x : v) {
            ++count_v;
        }

        assert(count_cv == 3 && count_v == 3);
    }
    Tracer::expect(3,0,0,0,0,3,"All element must be moved, not copied");


    // Test 9
    Tracer::reset();
    {
        static_vector<int, 4> v;

        v.try_emplace_back(1);
        v.try_emplace_back(2);
        v.try_emplace_back(3);
        v.try_emplace_back(4);
        
        int sum = 0;

        for (auto& x : v) {
            sum += x;
        }

        assert(v.full() && sum == 10);

        std::printf("Test 9: sum = %d\n", sum);
    }


    // Test 10
    Tracer::reset();
    {
        static_vector<Tracer, 4> v;

        Tracer* p1 = v.try_emplace_back(0, 0.0);
        Tracer* p2 = v.try_emplace_back(1, 0.0);
        v.try_emplace_back(2, 0.0);
        v.try_emplace_back(3, 0.0);
        Tracer* p5 = v.try_emplace_back(4, 0.0);

        assert(p5 == nullptr);
        assert((char*)p2 - (char*)p1 == sizeof(Tracer));
        std::printf("%d\n", v.full());
    }
    Tracer::expect(4,0,0,0,0,4,"fill to capacity");


    // Test 11
    Tracer::reset();
    {
        static_vector<Tracer, 4> a;
        a.try_emplace_back(0, 0.0);
        a.try_emplace_back(1, 0.0);
        a.try_emplace_back(2, 0.0);
        static_vector<Tracer, 4> b(a);
        b[0].id = 99;
        assert(a[0].id == 0); // check that a[0].id remains unchanged
    }
    Tracer::expect(3,3,0,0,0,6,"Test the Copy Constructor");


    // Test 12
    Tracer::reset();
    {
        static_vector<Tracer, 4> a;
        a.try_emplace_back(0, 0.0);
        a.try_emplace_back(1, 0.0);
        a.try_emplace_back(2, 0.0);
        static_vector<Tracer, 4> c(std::move(a));
        c[0].id = 99;
        assert(a.size() == 0); // check that move constructor worked
    }
    Tracer::expect(3,0,3,0,0,6,"Test the Move Constructor");


    // Test 13
    Tracer::reset();
    {
        static_vector<Tracer, 4> a;
        a.try_emplace_back(0, 0.0);
        a.try_emplace_back(1, 0.0);
        a.try_emplace_back(2, 0.0);
        static_vector<Tracer, 4> b;
        b.try_emplace_back(99, 0.0);
        b.try_emplace_back(100, 0.0);
        b = a;
        assert(b.size() == 3); // check that copy assignment worked
        Tracer::expect(5,3,0,0,0,2,"Test the Copy Assignment");
    }


    // Test 14
    Tracer::reset();
    {
        static_vector<Tracer, 4> a;
        a.try_emplace_back(0, 0.0);
        a.try_emplace_back(1, 0.0);
        a.try_emplace_back(2, 0.0);
        static_vector<Tracer, 4> c;
        c.try_emplace_back(99, 0.0);
        c = std::move(a);
        assert(a.size() == 0); // check that move assignment worked
        Tracer::expect(4,0,3,0,0,4,"Test the Move Assignment");
    }


    // Test 15
    Tracer::reset();
    {
        static_vector<Tracer, 4> b;
        b.try_emplace_back(0, 0.0);
        b.try_emplace_back(1, 0.0);
        b = b;
        assert(b.size() == 2); // edge case: self-copy assignment
        Tracer::expect(2,0,0,0,0,0,"Test the Self Copy Assignment");
    }


    // Test 16: a constructor throws inside try_emplace_back (experiment 1)
    Thrower::countdown = -1;
    {
        static_vector<Thrower, 4> v;
        v.try_emplace_back("payload number one, long");
        v.try_emplace_back("payload number two, long");

        bool caught = false;
        Thrower::countdown = 0;                     // the very next Thrower constructor throws
        try {
            v.try_emplace_back("payload number three, long");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        Thrower::countdown = -1;                    // back to "never"

        std::printf("%s 16a throw in try_emplace_back: caught=%d size=%zu live=%d\n",
                    (caught && v.size() == 2 && Thrower::live == 2) ? "[PASS]" : "[FAIL]",
                    caught, v.size(), Thrower::live);
    }
    std::printf("%s 16b after the scope: live=%d\n",
                Thrower::live == 0 ? "[PASS]" : "[FAIL]", Thrower::live);


    // Test 17: the copy constructor throws on the third element (experiment 3, with the fix)
    {
        static_vector<Thrower, 4> a;
        a.try_emplace_back("payload number one, long");
        a.try_emplace_back("payload number two, long");
        a.try_emplace_back("payload number three, long");

        bool caught = false;
        Thrower::countdown = 2;                     // copies 1 and 2 succeed, copy 3 throws
        try {
            static_vector<Thrower, 4> b(a);
        } catch (const std::runtime_error&) {
            caught = true;
        }
        Thrower::countdown = -1;

        std::printf("%s 17a copy throws halfway: caught=%d a.size=%zu live=%d\n",
                    (caught && a.size() == 3 && Thrower::live == 3) ? "[PASS]" : "[FAIL]",
                    caught, a.size(), Thrower::live);
    }
    std::printf("%s 17b after the scope: live=%d\n",
                Thrower::live == 0 ? "[PASS]" : "[FAIL]", Thrower::live);
}