#ifndef AL_MALLOC_H
#define AL_MALLOC_H

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "pragmadefs.h"


namespace gsl {
template<typename T> using owner = T;
};

inline void al_free(size_t alignment, gsl::owner<void*> ptr) noexcept
{ ::operator delete[](ptr, std::align_val_t{alignment}); }
[[gnu::alloc_align(1), gnu::alloc_size(2), gnu::malloc]]
gsl::owner<void*> al_calloc(size_t alignment, size_t size);


#define DISABLE_ALLOC                                                         \
    void *operator new(size_t) = delete;                                      \
    void *operator new[](size_t) = delete;                                    \
    void operator delete(void*) noexcept = delete;                            \
    void operator delete[](void*) noexcept = delete;


enum FamCount : size_t { };

#define DEF_FAM_NEWDEL(T, FamMem)                                             \
    static constexpr size_t Sizeof(size_t count) noexcept                     \
    {                                                                         \
        static_assert(&Sizeof == &T::Sizeof,                                  \
            "Incorrect container type specified");                            \
        return std::max(decltype(FamMem)::Sizeof(count, offsetof(T, FamMem)), \
            sizeof(T));                                                       \
    }                                                                         \
                                                                              \
    gsl::owner<void*> operator new(size_t /*size*/, FamCount count)           \
    {                                                                         \
        const auto alignment = std::align_val_t{alignof(T)};                  \
        return ::operator new[](T::Sizeof(count), alignment);                 \
    }                                                                         \
    void operator delete(gsl::owner<void*> block, FamCount) noexcept          \
    { ::operator delete[](block, std::align_val_t{alignof(T)}); }             \
    void operator delete(gsl::owner<void*> block) noexcept                    \
    { ::operator delete[](block, std::align_val_t{alignof(T)}); }             \
    void *operator new[](size_t /*size*/) = delete;                           \
    void operator delete[](void* /*block*/) = delete;


namespace al {

template<typename T, std::size_t AlignV=alignof(T)>
struct allocator {
    static constexpr auto Alignment = std::max(AlignV, alignof(T));
    static constexpr auto AlignVal = std::align_val_t{Alignment};

    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using is_always_equal = std::true_type;

    template<typename U>
    struct rebind {
        using other = std::enable_if_t<alignof(U) <= Alignment, allocator<U,Alignment>>;
    };

    constexpr explicit allocator() noexcept = default;
    template<typename U, std::size_t N>
    constexpr explicit allocator(const allocator<U,N>&) noexcept
    { static_assert(Alignment == allocator<U,N>::Alignment); }

    gsl::owner<T*> allocate(std::size_t n)
    {
        if(n > std::numeric_limits<std::size_t>::max()/sizeof(T)) throw std::bad_alloc();
        return static_cast<gsl::owner<T*>>(::operator new[](n*sizeof(T), AlignVal));
    }
    void deallocate(gsl::owner<T*> p, std::size_t) noexcept
    { ::operator delete[](gsl::owner<void*>{p}, AlignVal); }
};
template<typename T, std::size_t N, typename U, std::size_t M>
constexpr bool operator==(const allocator<T,N>&, const allocator<U,M>&) noexcept
{ return allocator<T,N>::Alignment == allocator<U,M>::Alignment; }
template<typename T, std::size_t N, typename U, std::size_t M>
constexpr bool operator!=(const allocator<T,N>&, const allocator<U,M>&) noexcept
{ return allocator<T,N>::Alignment != allocator<U,M>::Alignment; }


template<typename T>
constexpr T *to_address(T *p) noexcept
{
    static_assert(!std::is_function<T>::value, "Can't be a function type");
    return p;
}

template<typename T>
constexpr auto to_address(const T &p) noexcept
{
    return ::al::to_address(p.operator->());
}


template<typename T, typename ...Args>
constexpr T* construct_at(T *ptr, Args&& ...args)
    noexcept(std::is_nothrow_constructible_v<T, Args...>)
{
    /* NOLINTBEGIN(cppcoreguidelines-owning-memory) construct_at doesn't
     * necessarily handle the address from an owner, while placement new
     * expects to.
     */
    return ::new(static_cast<void*>(ptr)) T{std::forward<Args>(args)...};
    /* NOLINTEND(cppcoreguidelines-owning-memory) */
}

} // namespace al

#endif /* AL_MALLOC_H */
