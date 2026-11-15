// lesson_5_4_concepts.cpp — concepts & constraints: requires clauses and named concepts
//
// ONE idea: concepts let you name a set of requirements on a type and use that
// name to constrain templates — replacing hard-to-read SFINAE with clean,
// self-documenting constraints and vastly better compiler errors.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_5_4_concepts.cpp -o conc && ./conc

#include <concepts>
#include <iostream>
#include <string>
#include <type_traits>

// ============================================================
// 1. NAMED CONCEPTS
//    A concept is a compile-time predicate on a type: true or false.
// ============================================================

// Concept wrapping a type trait: true for integral types only.
template <class T>
concept MyIntegral = std::is_integral_v<T>;

// Concept with a requires expression: true if T supports + returning T.
template <class T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::same_as<T>;
};

// Concept with multiple requirements: true if T can be streamed to cout.
template <class T>
concept Printable = requires(T a) {
    { std::cout << a } -> std::same_as<std::ostream&>;
};

// ============================================================
// 2. CONSTRAINED FUNCTIONS — two spellings, same meaning
// ============================================================

// Spelling A: template <Concept T> — the concept names the parameter type.
template <MyIntegral T>
void show_integral(T x) {
    std::cout << "  [integral] " << x << "\n";
}

// Spelling B: requires clause — equivalent, useful for multiple constraints.
template <class T>
void show_integral_req(T x) requires MyIntegral<T> {
    std::cout << "  [requires] " << x << "\n";
}

// Constrained by Addable: only types that support + returning T.
template <Addable T>
T add_values(T a, T b) {
    return a + b;
}

// Constrained by Printable: only types that can be streamed to std::cout.
template <Printable T>
void show_printable(const T& x) {
    std::cout << "  [printable] " << x << "\n";
}

// ============================================================
// 3. STANDARD CONCEPTS (from <concepts>)
// ============================================================

template <class T>
void demo_standard_concepts() {
    static_assert(std::same_as<int, int>);
    static_assert(!std::same_as<int, double>);
    static_assert(std::convertible_to<int, double>);
    static_assert(std::integral<int>);
    static_assert(!std::integral<double>);
    (void)0;
}

int main() {
    std::cout << "=== Named concepts ===\n";
    show_integral(42);             // OK: int satisfies MyIntegral
    show_integral_req(7);          // OK: same constraint, requires-clause spelling
    // show_integral(3.14);        // ERROR: double is not MyIntegral — concept fails

    std::cout << "\n=== Addable concept ===\n";
    std::cout << "  add_values(3, 4)      = " << add_values(3, 4) << "\n";
    std::cout << "  add_values(1.5, 2.5)  = " << add_values(1.5, 2.5) << "\n";
    // add_values(std::string("a"), std::string("b")); // OK actually — string supports +

    std::cout << "\n=== Printable concept ===\n";
    show_printable(std::string("hello"));   // OK: string streams to cout
    show_printable(42);                     // OK: int streams to cout

    std::cout << "\n=== Standard concepts ===\n";
    demo_standard_concepts<int>();
    std::cout << "  same_as, convertible_to, integral verified via static_assert\n";

    return 0;
}
