#pragma once


#include <cstddef>   // for std::byte, std::size_t
#include <memory>    // for std::construct_at, std::destroy_at
#include <type_traits>   // for std::is_trivially_destructible_v<T>
#include <utility>



template <typename T, std::size_t N>
class static_vector {

    private:
        alignas(T) std::byte storage_[sizeof(T) * N];   // room for N T's, constructs none of them
        std::size_t size_ = 0;                          // slots 0 .. size_-1 hold live objects

        T* slot(std::size_t i) noexcept       
        { 
            // return the address of slot i, as a T*
            return reinterpret_cast<T*>(storage_ + (i * sizeof(T)));  // pointer arithmetic on bytes
        }

        const T* slot(std::size_t i) const noexcept 
        { 
            // return the same, read-only (const)
            return reinterpret_cast<const T*>(storage_ + (i * sizeof(T)));  // pointer arithmetic on bytes
        }

    public:

        // empty constructor (Special: for this static_vector don't use default cos default will zero-initialise the whole object but we want to no initialise at all for this static vector)
        static_vector() noexcept {

        }

        ~static_vector() { 
            clear();    
        }

    static_vector(const static_vector& other);
    static_vector& operator=(const static_vector& other);
    static_vector(static_vector&& other) noexcept(std::is_nothrow_move_constructible_v<T>);
    static_vector& operator=(static_vector&& other) noexcept(std::is_nothrow_move_constructible_v<T>);

        // build a new T directly in next free slot, using whatever constructor arguments I pass, with no copy and no move
        template <typename... Args>
        T* try_emplace_back(Args&&... args)            // nullptr when full: no exceptions
        {
            if (size_ == N) return nullptr; // edge case: alrdy full
            // std::construct_at uses the address u give it (in contrast "new" finds fresh heap memory)
            T* ptr = std::construct_at(slot(size_), std::forward<Args>(args)... );  // build a T at the first empty slot, passing the arguments through unchanged
            ++size_;   // Smart design: We increment size only after std::construct_at finish successfully so in case it throws, the size_ never overclaims an element that doesn't exist. The destructor later destroys size_ slots so this is important to avoid double-free
            return ptr;
        }

        void pop_back() noexcept
        {
            --size_;
            std::destroy_at(slot(size_));
        }

        void clear() noexcept
        {
            // "if constexpr" can only be used when condition can be determined at compile time e.g. type_traits stuff AND "if constexpr" only discards losing branch when it is inside a Template (so it is pointless to use "if constexpr" when not in a Template, u might as well just use normal if statement for those)
            // why we use "if constexpr": Normal if statements compile all branches during compile time (may cause compile error if 1 branch might) vs "if constexpr" compiles only the winning chosen branch (based on condition = true/false)
            // In this case: if std::is_trivially_destructible_v<T> = true, this entire if statement ceases to exist (deleted by compiler and never compiled so no compile error possible)
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (std::size_t i = 0; i < size_; ++i) {
                    std::destroy_at(slot(i));
                }
            }

            size_ = 0;   
        }

        T& operator[](std::size_t i) noexcept 
        {
            return *slot(i);
        }
        
        // same as above function, but read-only
        const T& operator[](std::size_t i) const noexcept
        {
            return *slot(i);
        }

        T* begin() noexcept
        {
            return slot(0);
        } 

        T* end() noexcept
        {
            // the reason why "end" points to one slot PAST last element is cos we often do e.g. for (auto it = v.begin(); it != v.end(); ++it)
            return slot(size_);  // by convention, "end" returns a pointer one spot past last element [slot(size_ - 1) is last element, slot(size_) is non-existent element that is 1 spot past end]
        }

        const T* begin() const noexcept
        {
            return slot(0);
        }

        const T* end() const noexcept
        {
            // the reason why "end" points to one slot PAST last element is cos we often do e.g. for (auto it = v.begin(); it != v.end(); ++it)
            return slot(size_);  // by convention, "end" returns a pointer one spot past last element [slot(size_ - 1) is last element, slot(size_) is non-existent element that is 1 spot past end]
        }

        std::size_t size() const noexcept
        {
            return size_;
        }

        static constexpr std::size_t capacity() noexcept 
        { 
            return N; 
        }

        bool empty() const noexcept
        {
            return size_ == 0;
        }
        bool full() const noexcept
        {
            return size_ == N;
        }
};
