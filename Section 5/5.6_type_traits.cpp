// lesson_5_6_type_traits.cpp — type traits: is_*, enable_if_t, void_t, decay, remove_reference
//
// ONE idea: type traits let you inspect and transform types at compile time —
// ask "is this type integral?", "strip this reference", "decay this array",
// or "remove this pointer" — all without running a single instruction.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_5_6_type_traits.cpp -o tt && ./tt

#include <cstddef>
#include <memory>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

// ============================================================
// 1. Type property queries: std::is_*_v
//    Compile-time yes/no questions about a type.
// ============================================================

static_assert(std::is_integral_v<int>);
static_assert(std::is_integral_v<char>);
static_assert(!std::is_integral_v<double>);
static_assert(std::is_floating_point_v<double>);
static_assert(!std::is_floating_point_v<int>);
static_assert(std::is_same_v<int, int>);
static_assert(!std::is_same_v<int, long>);
static_assert(std::is_pointer_v<int*>);
static_assert(!std::is_pointer_v<int>);
static_assert(std::is_array_v<int[5]>);
static_assert(!std::is_array_v<int*>);
static_assert(std::is_move_constructible_v<std::string>);

// ============================================================
// 2. type transformations
//    Remove references, strip const/volatile, decay arrays/functions to pointers.
// ============================================================

static_assert(std::is_same_v<std::remove_reference_t<int&>, int>);
static_assert(std::is_same_v<std::remove_reference_t<const int&>, const int>);
static_assert(std::is_same_v<std::remove_cv_t<const int>, int>);
static_assert(std::is_same_v<std::decay_t<int[5]>, int*>);        // array -> pointer
static_assert(std::is_same_v<std::decay_t<int(&)[5]>, int*>);     // ref-to-array -> pointer
static_assert(std::is_same_v<std::decay_t<int>, int>);            // plain type unchanged
static_assert(std::is_same_v<std::decay_t<int&>, int>);           // reference stripped

// ============================================================
// 3. void_t: the detection idiom
//    Check whether an expression is valid for a type.
// ============================================================

template <class T, class = void>
struct has_plus : std::false_type { };

template <class T>
struct has_plus<T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>>
    : std::true_type { };

static_assert(has_plus<int>::value);
static_assert(has_plus<std::string>::value);
static_assert(!has_plus<std::unique_ptr<int>>::value);

// ============================================================
// 4. enable_if_t: SFINAE — remove overloads that don't match
//    (The pre-concepts way; concepts do this more readably, but enable_if is
//    still used in detection idioms and legacy code.)
// ============================================================

template <class T>
std::enable_if_t<std::is_integral_v<T>, void>
print_kind(T x) {
    std::cout << "  integral: " << x << "\n";
}

template <class T>
std::enable_if_t<std::is_floating_point_v<T>, void>
print_kind(T x) {
    std::cout << "  floating: " << x << "\n";
}

// ============================================================
// 5. Practical use: choose behavior based on whether a type is trivial to copy
// ============================================================

template <class T>
void copy_or_move(T& dst, const T& src) {
    if constexpr (std::is_trivially_copyable_v<T>) {
        // trivially copyable: a memcpy-style copy is fine
        dst = src;
        std::cout << "  trivially copyable: plain assignment\n";
    } else {
        // not trivially copyable: respect the copy constructor
        dst = src;
        std::cout << "  not trivially copyable: copy constructor path\n";
    }
}

int main() {
    std::cout << "=== Type property queries (is_*_v) ===\n";
    std::cout << "  is_integral<int>:        " << std::is_integral_v<int> << "\n";
    std::cout << "  is_integral<double>:     " << std::is_integral_v<double> << "\n";
    std::cout << "  is_floating_point<double>: " << std::is_floating_point_v<double> << "\n";
    std::cout << "  is_same<int, long>:      " << std::is_same_v<int, long> << "\n";
    std::cout << "  is_pointer<int*>:        " << std::is_pointer_v<int*> << "\n";
    std::cout << "  is_array<int[5]>:        " << std::is_array_v<int[5]> << "\n";

    std::cout << "\n=== Type transformations ===\n";
    std::cout << "  remove_reference_t<int&>:          "
              << std::is_same_v<std::remove_reference_t<int&>, int> << "\n";
    std::cout << "  decay_t<int[5]> is int*:           "
              << std::is_same_v<std::decay_t<int[5]>, int*> << "\n";
    std::cout << "  decay_t<int&> is int:              "
              << std::is_same_v<std::decay_t<int&>, int> << "\n";

    std::cout << "\n=== Detection idiom (void_t): has operator+ ===\n";
    std::cout << "  has_plus<int>:          " << has_plus<int>::value << "\n";
    std::cout << "  has_plus<string>:       " << has_plus<std::string>::value << "\n";
    std::cout << "  has_plus<unique_ptr>:   " << has_plus<std::unique_ptr<int>>::value << "\n";

    std::cout << "\n=== enable_if_t (SFINAE): print_kind ===\n";
    print_kind(42);
    print_kind(3.14);
    // print_kind(std::string("hi"));   // ERROR: neither enable_if matches

    std::cout << "\n=== if constexpr on is_trivially_copyable_v ===\n";
    int a = 0, b = 7;
    copy_or_move(a, b);
    std::string s1, s2 = "hello";
    copy_or_move(s1, s2);

    return 0;
}
