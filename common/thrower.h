#pragma once

#include <stdexcept>
#include <string>

struct Thrower {
    static inline int live = 0;            // fully built and not yet destroyed
    static inline int countdown = -1;      // throw when this reaches 0; -1 means never
    std::string payload;                   // over 15 chars, so it owns heap memory ASan can see
    explicit Thrower(const char* s) : payload(s) { maybe_throw(); ++live; }
    Thrower(const Thrower& o) : payload(o.payload) { maybe_throw(); ++live; }
    ~Thrower() { --live; }
    static void maybe_throw() { if (countdown >= 0 && countdown-- == 0) throw std::runtime_error("boom"); }
};
