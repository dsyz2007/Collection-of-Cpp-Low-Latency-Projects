#pragma once

#include <utility>
#include <type_traits>
#include <cstddef>




template<typename T>
class Unique {
    private:
        T* ptr_ = nullptr;      

    public:
        // empty-constructor (Constructors don't have return types at all)
        Unique() noexcept = default;

        // non-empty constructor (Constructors don't have return types at all)
        explicit Unique(T* p) noexcept;
        
        // destructor
        ~Unique();

        // delete copy-constructor (don't allow it)
        Unique(const Unique& other) = delete;

        // delete copy-assigment (don't allow it)
        Unique& operator=(const Unique& other) = delete;

        // move-constructor
        Unique(Unique&& other) noexcept; // "other" cannot be const cos u must set to nullptr after move to prevent potential double-free

        // move-assigment
        Unique& operator=(Unique&& other) noexcept; // "other" cannot be const cos u must set to nullptr after move to prevent potential double-free
        
        // overload for * operator
        T& operator*() const noexcept;
        
        // overload for -> operator ("->" is essentially the same as (*pointer).member)
        T* operator->() const noexcept;

        // get()
        T* get() const noexcept;

        // bool()
        explicit operator bool() const noexcept;

        // release()
        T* release() noexcept;

        // reset()
        void reset(T* p = nullptr) noexcept;

        // swap()
        void swap(Unique& other) noexcept;

};



// non-empty constructor
template<typename T>
Unique<T>::Unique(T* p) noexcept : ptr_{p}
{

}


// destructor
template<typename T>
Unique<T>::~Unique(){
    delete ptr_; // fun fact: this deletes the tracer object that ptr_ points to, "ptr_" still exists after this, just that it points to freed unallocated space now
    ptr_ = nullptr;
}


// move-constructor
template<typename T>
Unique<T>::Unique(Unique&& other) noexcept : ptr_{std::move(other.ptr_)}
{
    other.ptr_ = nullptr;
}


// move-assignment
template<typename T>
Unique<T>& Unique<T>::operator=(Unique&& other) noexcept
{
    if (this == &other) return *this; // edge case (check by comparing address): self-move
    delete ptr_; // IMPORTANT: this's ptr_ may alrdy own something which must be deleted first to prevent leaks
    ptr_ = std::move(other.ptr_);
    other.ptr_ = nullptr;
    return *this;
}


// overload for * operator
template<typename T>
T& Unique<T>::operator*() const noexcept 
{
    return *ptr_;
}


// overload for -> operator ("->" is essentially the same as (*pointer).member). Rule: All "->" operators must return type T* (it is a C++ rule!)
template<typename T>
T* Unique<T>::operator->() const noexcept 
{
    return ptr_;
}


// get()
template<typename T>
T* Unique<T>::get() const noexcept
{
    return ptr_;
}


// bool() [This is a conversion operator, no return type needed since its name alrdy says what it returns]
template<typename T>
Unique<T>::operator bool() const noexcept{
    return ptr_ != nullptr;
}


// release()
template<typename T>
T* Unique<T>::release() noexcept
{
    T* temp = ptr_;
    ptr_ = nullptr;
    return temp;
}


// reset()
template<typename T>
void Unique<T>::reset(T* p) noexcept
{
    T* temp = ptr_;
    ptr_ = p;
    delete temp; // IMPORTANT: Prevent Memory Leak (detectable with ASan build)
}


// swap()
template<typename T>
void Unique<T>::swap(Unique& other) noexcept
{
    T* temp = ptr_;
    ptr_ = other.ptr_;
    other.ptr_ = temp;
}



// external (not a member function) make_unique for single object
template<typename T, typename... Args> requires (!std::is_array_v<T>)
Unique<T> make_unique(Args&&... args) 
{
    return Unique<T>(new T(std::forward<Args>(args)...));
}





template<typename T>
class Unique<T[]> {
    private:
        T* ptr_ = nullptr;      

    public:
        // empty-constructor (Constructors don't have return types at all)
        Unique() noexcept = default;

        // non-empty constructor (Constructors don't have return types at all)
        explicit Unique(T* p) noexcept;
        
        // destructor
        ~Unique();

        // delete copy-constructor (don't allow it)
        Unique(const Unique& other) = delete;

        // delete copy-assigment (don't allow it)
        Unique& operator=(const Unique& other) = delete;

        // move-constructor
        Unique(Unique&& other) noexcept; // "other" cannot be const cos u must set to nullptr after move to prevent potential double-free

        // move-assigment
        Unique& operator=(Unique&& other) noexcept; // "other" cannot be const cos u must set to nullptr after move to prevent potential double-free

        // overload for [] operator
        T& operator[](std::size_t i) const noexcept;

        // get()
        T* get() const noexcept;

        // bool()
        explicit operator bool() const noexcept;

        // release()
        T* release() noexcept;

        // reset()
        void reset(T* p = nullptr) noexcept;

        // swap()
        void swap(Unique& other) noexcept;

};


// non-empty constructor
template<typename T>
Unique<T[]>::Unique(T* p) noexcept : ptr_{p}
{

}


// destructor
template<typename T>
Unique<T[]>::~Unique() 
{
    delete[] ptr_;
}


// overload for [] operator
template<typename T>
T& Unique<T[]>::operator[](std::size_t i) const noexcept
{
    return ptr_[i];
}


// get()
template<typename T>
T* Unique<T[]>::get() const noexcept
{
    return ptr_;
}


// overload for bool operator (no need return type cos bool in the name means confirm return bool)
template<typename T>
Unique<T[]>::operator bool() const noexcept
{
    return ptr_ != nullptr;
}


// move-constructor
template<typename T>
Unique<T[]>::Unique(Unique<T[]>&& other) noexcept : ptr_{std::move(other.ptr_)}
{
    other.ptr_ = nullptr;
}


// move assignment
template<typename T>
Unique<T[]>& Unique<T[]>::operator=(Unique<T[]>&& other) noexcept
{
    if(this == &other) return *this; // edge case (check by comparing address): self-move
    T* temp = ptr_;
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    delete[] temp; // IMPORTANT: Prevent Memory Leak (detectable with ASan build)
    return *this;
}


// release()
template<typename T>
T* Unique<T[]>::release() noexcept
{
    T* temp = ptr_;
    ptr_ = nullptr;
    return temp;
}


// reset()
template<typename T>
void Unique<T[]>::reset(T* p) noexcept
{
    T* temp = ptr_;
    ptr_ = p;
    delete[] temp; // IMPORTANT: Prevent Memory Leak (detectable with ASan build)
}


// swap()
template<typename T>
void Unique<T[]>::swap(Unique<T[]>& other) noexcept
{
    T* temp = ptr_;
    ptr_ = other.ptr_;
    other.ptr_ = temp;
}




// external (not a member function) make_unique for array of objects
template<typename T> requires(std::is_unbounded_array_v<T>)
Unique<T> make_unique(std::size_t n) 
{
    return Unique<T>(new std::remove_extent_t<T>[n]());
}



