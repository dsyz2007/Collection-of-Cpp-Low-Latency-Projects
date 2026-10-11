#include "../Unique.h"


int use_raw(int v)    { int* p = new int(v); int r = *p + 1; delete p; return r; }
int use_unique(int v) { auto u = make_unique<int>(v); return *u + 1; }



struct Trivial    { int* p; };                            // trivial destructor
struct NonTrivial { int* p; ~NonTrivial() { delete p; } }; // non-trivial
int f_raw(int* p)           { return *p + 1; }
int f_trivial(Trivial t)    { return *t.p + 1; }
int f_nontrivial(NonTrivial t) { return *t.p + 1; }



