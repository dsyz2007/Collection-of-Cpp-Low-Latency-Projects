#include "static_vector.h"
#include "../common/tracer.h"

#include <cstddef>
#include <cassert>
#include <utility>   // for std::move()


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


}