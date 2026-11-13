#include <iostream>
#include <utility>    // for std::swap(), std::move()

// for this custom shared pointer design, the drawback is you must only create Shared objects for the target object by copying/moving existing Shared objects pointing to that same target object (the only exception is the first time for a target object) in order for this Shared pointer to work
template<typename T>
class Shared {
private:
    T* ptr = nullptr;
    int* count = nullptr; // smart design trick: count is a pointer, not a variable
    int* watchers = nullptr; // how many Weak objects are watching that object of type T
    void DeleteObj() noexcept {  // delete object if count is zero
        if (count == nullptr) return; // no object to delete and free memory
        --(*count);
        if ((*count) == 0) {
            delete ptr;  // delete T-object
            --(*watchers);  // all the Shared object owners are gone (count = 0)
            if ((*watchers) == 0) {  // recall that the Shared owners collectively count as 1 watcher
                delete count;  // don't delete unless no Weak watching it so that any Weak that needs count will be able to answer their use_count() query
                delete watchers;  // there may be no watchers created for T_object at all so may need to free watchers memory in Shared object
            }
        }
        ptr = nullptr;
        count = nullptr;
        watchers = nullptr;
    }

public:
    // empty constructor (can use default keyword) [don't use explicit so that e.g. return {}, Book b{}, Shared<Order> s = {} would still work]
    Shared() noexcept = default;

    // non-empty constructor
    explicit Shared(T* obj) {
        count = new int(1); // heap allocation using "new"
        watchers = new int(1); // the Shared owners collectively count as 1 watcher
        ptr = obj;
    }

    // non-empty constructor (for use in Weak's lock() function only)
    explicit Shared(T* obj, int* c, int* w) {
        count = c; 
        watchers = w;
        ptr = obj;
        ++(*count); // rmbr to increment 1 to count after lock() from Weak pointer
    }

    // copy-constructor
    Shared(const Shared& other) noexcept : ptr{other.ptr}, count{other.count}, watchers{other.watchers} {
        if (count != nullptr) {
            ++(*count);
        }
    }

    // move-constructor
    Shared(Shared&& other) noexcept : ptr{std::move(other.ptr)}, count{std::move(other.count)}, watchers{std::move(other.watchers)} {
        other.ptr = nullptr;
        other.count = nullptr;
        other.watchers = nullptr;
    }

    // copy-assignment
    Shared& operator=(const Shared& other) noexcept {
        if (this == &other) return *this; // edge-case
        Shared temp = other; // follows copy-and-swap algo (Step 1: make independent copy of obj, Step 2: swap the individual data members (don't swap the object itself, std::swap() will actually call the move assignment function recursively infinitely until stackoverflow), Step 3: the copy is destroyed at end of scope [closing brace])
        std::swap(temp.ptr, ptr);
        std::swap(temp.count, count);
        std::swap(temp.watchers, watchers);
        return *this;
    } // temp will be destroyed by this closing brace (go out of scope RAII)

    // move-assignment
    Shared& operator=(Shared&& other) noexcept {
        if (this == &other) return *this; // edge case
        Shared temp = std::move(other); // follows move-and-swap algo (Step 1: make independent copy (moved) of obj, Step 2: swap the individual data members (don't swap the object itself, std::swap() will actually call the move assignment function recursively infinitely until stackoverflow), Step 3: the copy is destroyed at end of scope [closing brace])
        std::swap(temp.ptr, ptr);
        std::swap(temp.count, count);
        std::swap(temp.watchers, watchers);
        return *this;
    } // temp will be destroyed by this closing brace (go out of scope RAII)

    // destructor
    ~Shared() {
        DeleteObj();
    }

    // reset
    void reset() noexcept {
        DeleteObj();
    }

    // use_count() [returns num of Shared objects owning the T_object]
    int use_count() const noexcept {
        if (count == nullptr) return 0;
        else return *count;
    }

    // return_count_pointer()
    int* return_count_pointer() const noexcept {
        return count;
    }

    // return_watchers_pointer()
    int* return_watchers_pointer() const noexcept {
        return watchers;
    }

    // overload for bool operator
    operator bool() const noexcept {
        return ptr != nullptr;
    }

    // overload for "->" operator
    T* operator->() const noexcept {
        return ptr;
    }

    // overload for "*" operator
    T& operator*() const noexcept {
        return *ptr;
    }

    // get()
    T* get() const noexcept {    // I chose return type T* instead of T& so that if nullptr can still return
        return ptr;
    }
};



// Weak pointer
template<typename T>
class Weak {
private:
    T* ptr = nullptr;
    int* count = nullptr;
    int* watchers = nullptr;
    void DeleteObj() noexcept {  // delete both watcher and count if watcher is zero
        if (watchers == nullptr) return; // no object to delete and free memory
        --(*watchers);
        if ((*watchers) == 0 && (*count) == 0) {
            delete watchers;
            delete count;
        }
        ptr = nullptr;
        watchers = nullptr;
        count = nullptr;
    }

public:
    // empty constructor (can use default keyword) [don't use explicit so that e.g. return {}, Book b{}, Weak<Order> w = {} would still work]
    Weak() noexcept = default;

    // non-empty constructor
    Weak(const Shared<T>& s) noexcept : ptr{s.get()}, count{s.return_count_pointer()}, watchers{s.return_watchers_pointer()}
    {
        if (watchers != nullptr) ++(*watchers);
    }

    // copy constructor
    Weak(const Weak& other) noexcept : ptr{other.ptr}, count{other.count}, watchers{other.watchers}
    {
        if (watchers != nullptr) ++(*watchers);
    }

    // move constructor
    Weak(Weak&& other) noexcept : ptr{std::move(other.ptr)}, count{std::move(other.count)}, watchers{std::move(other.watchers)}
    {
        other.ptr = nullptr;
        other.count = nullptr;
        other.watchers = nullptr;
    }

    // copy-assignment
    Weak& operator=(const Weak& other) noexcept {
        if (this == &other) return *this;  // edge case
        Weak temp = other;
        std::swap(temp.ptr, ptr);
        std::swap(temp.count, count);
        std::swap(temp.watchers, watchers);
        return *this;
    }

    // move-assignment
    Weak& operator=(Weak&& other) noexcept {
        if (this == &other) return *this;  // edge case
        Weak temp = std::move(other); //invokes move constructor 
        std::swap(temp.ptr, ptr);
        std::swap(temp.count, count);
        std::swap(temp.watchers, watchers);
        return *this;
    }

    // destructor
    ~Weak() {
        DeleteObj();
    }

    // reset()
    void reset() noexcept {
        DeleteObj();
    }

    // use_count() [returns num of Shared objects owning the T_object]
    int use_count() const noexcept {
        if (count == nullptr) return 0;
        else return (*count);
    }

    // expired()
    bool expired() const noexcept {
        if(use_count() == 0) return true;
        else return false;
    }

    // lock()
    Shared<T> lock() {
        if (!expired()) { // T_object still exist
            return Shared(ptr, count, watchers);
        } else {
            return Shared<T>();
        }
    }
};



struct Order { int id; };

int main() {
    Shared<Order> a(new Order{1});     // a owns the Order
    Shared<Order> b = a;               // a and b both own it; no Order is copied
    a.use_count();                     // 2
    b->id;                             // 1: b works like a pointer, and so does (*b).id
    a.reset();                         // a lets go: a is empty, the Order lives on because b owns it
    b.use_count();                     // 1
    Shared<Order> c = std::move(b);    // c takes over: b is empty, the count is still 1
    if (c) {
        std::cout << "c holds something!\n";
    } else {
        std::cout << "Nope, c holds nothing.\n";
    }
    c.reset();                         // the last owner lets go: the Order is destroyed on this line


    Shared<Order> s(new Order{7});
    Weak<Order> w = s;                 // w watches the Order but doesn't own it
    s.use_count();                     // 1: a watcher isn't an owner
    w.expired();                       // false: the Order is alive

    {
        Shared<Order> t = w.lock();    // try to become an owner: works, because the Order is alive
        t->id;                         // 7
        s.use_count();                 // 2 while t exists
    }

    s.reset();                         // the last owner lets go: the Order is destroyed on THIS line,
                                   // even though w still watches it
    w.expired();                       // true
    Shared<Order> u = w.lock();        // doesn't work: u is empty (get() null, use_count() 0)
}