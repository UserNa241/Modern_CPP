// lesson_5_2_deduction.cpp — template argument deduction: decay, references, arrays, CTAD
//
// ONE idea: how the compiler deduces T depends on HOW the parameter is declared.
// Pass by value decays arrays and strips references; pass by reference preserves them;
// T&& is a forwarding reference that binds to both lvalues and rvalues.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_5_2_deduction.cpp -o ded && ./ded

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

// 1. Pass by value: decays arrays to pointers, strips top-level const/volatile and references.
template <class T>
void by_value(T x) { (void)x;
    std::cout << "    T deduced as ";
    if constexpr (std::is_same_v<T, int>) std::cout << "int\n";
    else if constexpr (std::is_same_v<T, int*>) std::cout << "int* (array decayed to pointer)\n";
    else if constexpr (std::is_same_v<T, const int>) std::cout << "const int (top const stripped)\n";
    else if constexpr (std::is_same_v<T, std::string>) std::cout << "std::string\n";
}

// 2. Pass by lvalue reference: T& deduces T as the referred-to type; references are preserved.
template <class T>
void by_lref(T& x) { (void)x;
    std::cout << "    T deduced as ";
    if constexpr (std::is_same_v<T, int>) std::cout << "int\n";
    else if constexpr (std::is_same_v<T, int&>) std::cout << "int& (lvalue reference preserved)\n";
}

// 3. Pass by rvalue reference: T&& is a FORWARDING REFERENCE.
//    Lvalue argument  -> T deduces as int&  (reference collapsing: int& && = int&)
//    Rvalue argument  -> T deduces as int   (stays int&&)
template <class T>
void by_rref(T&& x) { (void)x;
    std::cout << "    T deduced as ";
    if constexpr (std::is_same_v<T, int>) std::cout << "int (rvalue argument -> int&&)\n";
    else if constexpr (std::is_same_v<T, int&>) std::cout << "int& (lvalue argument -> lvalue ref)\n";
    else if constexpr (std::is_same_v<T, const int&>) std::cout << "const int& (const lvalue argument)\n";
}

// 4. const T& binds to anything: const lvalue, non-const lvalue, or rvalue.
template <class T>
void by_cref(const T& x) { (void)x;
    std::cout << "    T deduced as ";
    if constexpr (std::is_same_v<T, int>) std::cout << "int\n";
    else if constexpr (std::is_same_v<T, std::string>) std::cout << "std::string\n";
}

// 5. Array bound by reference: NO decay — the array keeps its size.
template <class T, std::size_t N>
void array_ref(T (&arr)[N]) { (void)arr;
    std::cout << "    T = int, N = " << N << " (array bound by reference, no decay)\n";
}

// Class template with CTAD (C++17): deduction from constructor arguments.
template <class T>
struct Pair {
    T first;
    T second;
    Pair(T a, T b) : first(a), second(b) {}
};

// Explicit deduction guide (optional here; the compiler generates one implicitly).
template <class T>
Pair(T, T) -> Pair<T>;

int main() {
    int i = 42;
    const int ci = 7;
    int arr[5] = {1, 2, 3, 4, 5};
    std::string s = "hello";

    std::cout << "=== Pass by value (decays arrays, strips const/ref) ===\n";
    by_value(i);       // T = int
    by_value(ci);      // T = int (top-level const stripped)
    by_value(arr);     // T = int* (array decays to pointer)
    by_value(s);       // T = std::string

    std::cout << "\n=== Pass by lvalue reference (preserves lvalue-ness) ===\n";
    by_lref(i);        // T = int

    std::cout << "\n=== Pass by rvalue reference (forwarding reference) ===\n";
    by_rref(i);        // lvalue -> T = int&
    by_rref(42);       // rvalue -> T = int
    by_rref(ci);       // const lvalue -> T = const int&

    std::cout << "\n=== Pass by const T& (binds to anything) ===\n";
    by_cref(i);        // T = int
    by_cref(ci);       // T = int
    by_cref(42);       // T = int (binds to temporary)
    by_cref(s);        // T = std::string

    std::cout << "\n=== Array: pass by value vs pass by reference ===\n";
    by_value(arr);     // decays: T = int*
    array_ref(arr);    // no decay: T = int, N = 5

    std::cout << "\n=== CTAD: class template argument deduction ===\n";
    Pair p{1, 2};      // C++17 CTAD deduces Pair<int>
    static_assert(std::is_same_v<decltype(p), Pair<int>>);
    std::cout << "    Pair p{1, 2}: first = " << p.first
              << ", second = " << p.second << " (deduced Pair<int>)\n";

    return 0;
}
