// lesson_5_5_own_concepts.cpp — defining your own concepts: Allocator, TriviallyRelocatable, subsumption
//
// ONE idea: you can write your own concepts with `requires`, and when one concept
// implies another (subsumption), the compiler picks the more constrained overload
// automatically — clean, predictable, no SFINAE guessing.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_5_5_own_concepts.cpp -o ownc && ./ownc

#include <concepts>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <type_traits>

// ============================================================
// 1. CONCEPT: Moveable (baseline)
// ============================================================

template <class T>
concept Moveable = std::is_move_constructible_v<T>;

// ============================================================
// 2. CONCEPT: TriviallyRelocatable
//    Structurally contains Moveable<T>, so it subsumes Moveable.
//    A type is trivially relocatable if moving it is just a memcpy:
//    move-construct then destroy the source == bitwise copy.
// ============================================================

template <class T>
concept TriviallyRelocatable =
    Moveable<T> &&
    std::is_trivially_copyable_v<T> &&
    std::is_trivially_destructible_v<T>;

static_assert(TriviallyRelocatable<int>);
static_assert(TriviallyRelocatable<double>);
static_assert(!TriviallyRelocatable<std::string>);   // owns heap memory
static_assert(!TriviallyRelocatable<std::unique_ptr<int>>);

// ============================================================
// 3. CONCEPT: SimpleAllocator (requires expression)
//    A minimal allocator concept: value_type, allocate, deallocate.
// ============================================================

template <class A>
concept SimpleAllocator = requires(A a, std::size_t n) {
    typename A::value_type;
    { a.allocate(n) } -> std::same_as<typename A::value_type*>;
    { a.deallocate(nullptr, n) };
};

// A tiny allocator that satisfies SimpleAllocator.
struct MallocAllocator {
    using value_type = int;
    int* allocate(std::size_t n) {
        return static_cast<int*>(::operator new(n * sizeof(int)));
    }
    void deallocate(int* p, std::size_t) { ::operator delete(p); }
};
static_assert(SimpleAllocator<MallocAllocator>);

// ============================================================
// 4. SUBUMPTION: TriviallyRelocatable wins over Moveable
//    Two overloads of relocate. For TriviallyRelocatable types both match,
//    but TriviallyRelocatable subsumes Moveable, so the compiler picks it.
// ============================================================

template <class T>
void relocate(T* dst, T* src) requires Moveable<T> {
    std::cout << "    relocate<Moveable>: move-construct + destroy\n";
    new (dst) T(std::move(*src));
    src->~T();
}

template <class T>
void relocate(T* dst, T* src) requires TriviallyRelocatable<T> {
    std::cout << "    relocate<TriviallyRelocatable>: memcpy (bitwise copy)\n";
    std::memcpy(dst, src, sizeof(T));
}

template <class T>
void demo_relocate() {
    T a{}; T b{};
    relocate(&b, &a);
}

struct Trivial { int x; };                       // trivially relocatable
struct NonTrivial { std::string s; };            // not trivially relocatable

int main() {
    std::cout << "=== TriviallyRelocatable checks ===\n";
    std::cout << "  int:           " << TriviallyRelocatable<int> << "\n";
    std::cout << "  std::string:   " << TriviallyRelocatable<std::string> << "\n";
    std::cout << "  unique_ptr:    " << TriviallyRelocatable<std::unique_ptr<int>> << "\n";

    std::cout << "\n=== SimpleAllocator ===\n";
    MallocAllocator alc;
    int* p = alc.allocate(3);
    std::cout << "  allocated " << (p != nullptr) << " ints via SimpleAllocator\n";
    alc.deallocate(p, 3);

    std::cout << "\n=== Subsumption: TriviallyRelocatable wins over Moveable ===\n";
    std::cout << "  Trivial (int x):  ";
    demo_relocate<Trivial>();                        // picks memcpy version
    std::cout << "  NonTrivial (string): ";
    demo_relocate<NonTrivial>();                     // picks move-construct version

    return 0;
}
