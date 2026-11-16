// lesson_5_7_constexpr.cpp — compile-time computation: constexpr, consteval, constinit, if constexpr
//
// ONE idea: move computation from runtime to compile time. constexpr/consteval/constinit
// force evaluation when the program is built; if constexpr discards branches at compile time.
// No runtime cost, no runtime errors for what the compiler can see.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_5_7_constexpr.cpp -o ct && ./ct

#include <iostream>
#include <string>
#include <type_traits>

// ============================================================
// 1. constexpr: evaluable at compile time, usable at runtime too
// ============================================================

// Compile-time factorial. Can run at runtime OR be used in a constant expression.
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i) result *= i;
    return result;
}

// Constexpr variable: initialized at compile time.
constexpr int fact5 = factorial(5);   // 120, computed during compilation
static_assert(fact5 == 120);

// ============================================================
// 2. consteval: IMMEDIATE function — compile time ONLY (C++20)
// ============================================================

// This function MUST be evaluated at compile time. Calling it with a
// non-constant argument is a compile error.
consteval int square(int n) {
    return n * n;
}

constexpr int sq9 = square(9);   // OK: compile-time call
static_assert(sq9 == 81);

// square(runtime_value);   // ERROR: consteval requires constant expression

// ============================================================
// 3. constinit: static initialization must be constant (C++20)
// ============================================================

// Guarantees init happens during constant evaluation — no static init order fiasco.
constinit int global_counter = 0;

// ============================================================
// 4. if constexpr: compile-time branching (C++17)
// ============================================================

template <class T>
void describe_type() {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "    " << typeid(T).name() << " is integral\n";
    } else if constexpr (std::is_floating_point_v<T>) {
        std::cout << "    " << typeid(T).name() << " is floating point\n";
    } else {
        std::cout << "    " << typeid(T).name() << " is something else\n";
    }
}

// The discarded branch is not instantiated — this would fail to compile for
// non-integral types if it were a normal if (typeid name() etc. fine here, but
// the point stands for SFINAE-unfriendly code).
template <class T>
void only_if_integral(T x) {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "    integral value: " << x << "\n";
    }
    // No else needed — for non-integral T, this function body is empty.
}

int main() {
    std::cout << "=== constexpr ===\n";
    std::cout << "  factorial(5) = " << factorial(5) << "\n";
    std::cout << "  fact5 (compile-time const) = " << fact5 << "\n";

    int n = 6;                        // runtime variable
    std::cout << "  factorial(" << n << ") = " << factorial(n) << "\n";  // runtime OK too

    std::cout << "\n=== consteval ===\n";
    std::cout << "  square(9) = " << sq9 << " (compile-time only)\n";

    std::cout << "\n=== constinit ===\n";
    std::cout << "  global_counter = " << global_counter << " (constant-initialized)\n";

    std::cout << "\n=== if constexpr ===\n";
    describe_type<int>();
    describe_type<double>();
    describe_type<std::string>();

    std::cout << "\n=== only_if_integral ===\n";
    only_if_integral(42);      // prints the value
    only_if_integral(3.14);    // prints nothing — branch discarded

    return 0;
}
