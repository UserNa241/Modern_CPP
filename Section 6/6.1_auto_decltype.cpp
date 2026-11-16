// lesson_6_1_auto_decltype.cpp — auto & decltype, static_assert, noexcept
//
// ONE idea: `auto` deduces type from an initializer; `decltype` inspects the
// type of an expression without evaluating it; `static_assert` checks types at
// compile time; `noexcept` promises a function never throws. Together they let
// you write code that adapts to types you don't name, verified at build time.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_6_1_auto_decltype.cpp -o ad && ./ad

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

// ============================================================
// 1. auto: deduce type from initializer
//    Same deduction rules as template parameters (pass by value).
// ============================================================

auto make_int() { return 42; }            // return type deduced as int
auto make_string() { return std::string("hi"); }  // return type deduced as std::string

// ============================================================
// 2. noexcept: promise the function never throws
// ============================================================

int add_noexcept(int a, int b) noexcept {
    return a + b;
}

static_assert(noexcept(add_noexcept(1, 2)), "add_noexcept must be noexcept");

// ============================================================
// 3. decltype: inspect type of an expression without evaluating it
// ============================================================

int x = 7;
const int cx = 9;
int& ref = x;

using T1 = decltype(x);      // int
using T2 = decltype(cx);     // const int
using T3 = decltype(ref);    // int&
using T4 = decltype((x));    // int& — parenthesized expression is an lvalue

static_assert(std::is_same_v<T1, int>);
static_assert(std::is_same_v<T2, const int>);
static_assert(std::is_same_v<T3, int&>);
static_assert(std::is_same_v<T4, int&>);

// ============================================================
// 4. decltype on function call expressions (never evaluated)
// ============================================================

int add(int a, int b) { return a + b; }

using T5 = decltype(add(1, 2));   // int — the call is NOT evaluated
static_assert(std::is_same_v<T5, int>);

// ============================================================
// 5. decltype(auto): deduce like auto BUT keep decltype semantics
// ============================================================

int& get_ref(int& r) { return r; }
int  get_val() { return 7; }

auto  r1 = get_ref(x);      // int          — auto strips the reference
decltype(auto) r2 = get_ref(x);   // int&       — decltype(auto) keeps the reference

static_assert(std::is_same_v<decltype(r1), int>);
static_assert(std::is_same_v<decltype(r2), int&>);

// ============================================================
// 6. auto with containers: deduce element type indirectly
// ============================================================

template <class Container>
void print_first(const Container& c) {
    if (c.empty()) return;
    auto first = *c.begin();              // deduced element type
    std::cout << "  first = " << first << "\n";
}

int main() {
    std::cout << "=== auto deduction ===\n";
    auto a1 = 42;                          // int
    auto a2 = 3.14;                        // double
    auto a3 = std::string("hello");        // std::string
    auto a4 = make_int();                  // int
    std::cout << "  a1=" << a1 << " a2=" << a2 << " a3=" << a3 << " a4=" << a4 << "\n";

    // auto strips top-level const and references:
    const int ci = 7;
    auto a5 = ci;                          // int (const stripped)
    auto& a6 = ci;                         // const int& (reference kept)
    static_assert(std::is_same_v<decltype(a5), int>);
    static_assert(std::is_same_v<decltype(a6), const int&>);
    std::cout << "  a5 (auto from const int) is int: " << std::is_same_v<decltype(a5), int> << "\n";
    std::cout << "  a6 (auto& from const int) is const int&: " << std::is_same_v<decltype(a6), const int&> << "\n";

    std::cout << "\n=== decltype inspection ===\n";
    std::cout << "  decltype(x)      = int:           " << std::is_same_v<T1, int> << "\n";
    std::cout << "  decltype(cx)     = const int:   " << std::is_same_v<T2, const int> << "\n";
    std::cout << "  decltype(ref)    = int&:        " << std::is_same_v<T3, int&> << "\n";
    std::cout << "  decltype((x))    = int& (lvalue): " << std::is_same_v<T4, int&> << "\n";
    std::cout << "  decltype(add(1,2)) = int:   " << std::is_same_v<T5, int> << "\n";

    std::cout << "\n=== noexcept check ===\n";
    std::cout << "  add_noexcept is noexcept: " << noexcept(add_noexcept(1, 2)) << "\n";

    std::cout << "\n=== decltype(auto) keeps references ===\n";
    std::cout << "  r1 (auto)  is int:        " << std::is_same_v<decltype(r1), int> << "\n";
    std::cout << "  r2 (decltype(auto)) is int&: " << std::is_same_v<decltype(r2), int&> << "\n";
    ++r2;                                 // modifying through r2 modifies x
    std::cout << "  after ++r2, x = " << x << "   (r2 aliases x)\n";

    std::cout << "\n=== auto with containers ===\n";
    std::vector<int> v{10, 20, 30};
    print_first(v);

    return 0;
}
