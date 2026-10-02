#pragma once

#include <cstdio>
#include <utility>




struct Tracer {

    static inline int constructs   = 0;   // static inline: C++17, one
    static inline int copies       = 0;   // definition, no .cpp needed
    static inline int moves        = 0;
    static inline int copy_assigns = 0;
    static inline int move_assigns = 0;
    static inline int destroys     = 0;

    int    id     = 0;
    double weight = 0.0;

    explicit Tracer(int i = 0, double w = 0.0) noexcept;
    Tracer(const Tracer& other) noexcept;
    Tracer(Tracer&&) noexcept;
    Tracer& operator=(const Tracer&) noexcept;
    Tracer& operator=(Tracer&&) noexcept;
    ~Tracer();

    static void reset() noexcept;              // zero all six counters
    static bool expect(int c, int cp, int mv,  // print + compare, return
                       int ca, int ma, int d,  // false on mismatch
                       const char* what);
};





// non-empty constructor
inline Tracer::Tracer(int i, double w) noexcept : id{i}, weight{w}
{
    constructs += 1;
}


// copy constructor
inline Tracer::Tracer(const Tracer& other) noexcept : id{other.id}, weight{other.weight}
{
    copies += 1;
}


// move constructor
inline Tracer::Tracer(Tracer&& other) noexcept : id{std::move(other.id)}, weight{std::move(other.weight)}
{
    other.id = -1;
    other.weight = -1;

    moves += 1;
}


// copy assigment
inline Tracer& Tracer::operator=(const Tracer& other) noexcept
{
    id = other.id;
    weight = other.weight;

    copy_assigns += 1;

    return *this; // all assignment operators must return a 
}


// move assignment
inline Tracer& Tracer::operator=(Tracer&& other) noexcept
{
    id = std::move(other.id);
    weight = std::move(other.weight);
    
    other.id = -1;
    other.weight = -1;

    move_assigns += 1;

    return *this; // for chaining operations
}


// destructor (destructors are "noexcept" automatically so not necessary to write "noexcept" again)
inline Tracer::~Tracer(){ // an object's destructor will automatically delete "this" (current object) after code in destrtuctor body runs finish so never type "delete this" as it causes infinite recursion (stack overflow error) since delete calls destructor and then destructor's delete calls destructor again and so on infinitely. We only delete ptr_ in Unique.h's destructor is cos Unique object owns another object so must delete ptr_ (which deletes the Tracer object it owns), and notice we did not delete this also there since its always automatic for destructor.
    destroys += 1;
}


// reset()
inline void Tracer::reset() noexcept
{
    constructs = 0;
    copies = 0;
    moves = 0;
    copy_assigns = 0;
    move_assigns = 0;
    destroys = 0;
}


// expect()
inline bool Tracer::expect(int c, int cp, int mv, int ca, int ma, int d, const char* what)
{
    constexpr int n = 6;
    const int         actual[n]   = { constructs, copies, moves,  copy_assigns, move_assigns, destroys };
    const int         expected[n] = { c,          cp,     mv,     ca,           ma,           d        };
    const char* const names[n]    = { "ctor",     "copy", "move", "c_assign",   "m_assign",   "dtor"   };

    bool all_match = true;
    for (int i = 0; i < n; ++i) {
        if (actual[i] != expected[i]) {
            all_match = false;
        }
    }

    std::printf("%s %-24s", all_match ? "[PASS]" : "[FAIL]", what);
    for (int i = 0; i < n; ++i) {
        std::printf(" %s=%d", names[i], actual[i]);
        if (actual[i] != expected[i]) {
            std::printf("(expected %d)", expected[i]);
        }
    }
    std::printf("\n");

    return all_match;
}

