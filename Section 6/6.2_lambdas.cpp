// lesson_6_2_lambdas.cpp — range-based for loops & lambdas: capture modes, generic lambdas, this capture
//
// ONE idea: range-based for loops and lambdas are the two most-used modern C++
// features. Range-for desugars to begin()/end() iteration; lambdas are anonymous
// function objects with explicit capture, parameter, and body.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_6_2_lambdas.cpp -o lam && ./lam

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

// ============================================================
// 1. Range-based for loop: desugars to begin()/end() iteration
// ============================================================

void range_for_demo() {
    std::vector<int> v{1, 2, 3, 4, 5};

    // Range-for: equivalent to begin()/end() loop (C++11)
    int sum = 0;
    for (int x : v) {
        sum += x;
    }
    std::cout << "  sum = " << sum << "\n";

    // Modify elements through a reference
    for (int& x : v) {
        x *= 2;
    }
    std::cout << "  doubled: ";
    for (int x : v) {
        std::cout << x << " ";
    }
    std::cout << "\n";
}

// ============================================================
// 2. Lambda capture modes
// ============================================================

struct Counter {
    int value = 0;

    void demo() {
        int local = 100;

        // Capture by value: copies local at lambda creation
        auto by_value = [local]() {
            std::cout << "    by_value local = " << local << "\n";
            // local = 200;  // ERROR: local is const inside the lambda
        };

        // Capture by reference: aliases local
        auto by_ref = [&local]() {
            local = 200;  // OK: modifies the original
            std::cout << "    by_ref local = " << local << "\n";
        };

        // Capture all by value + this by reference: [=, this] (C++20 requires explicit this)
        auto capture_all_value = [=, this]() {
            std::cout << "    capture_all_value: local = " << local
                      << ", this->value = " << value << "\n";
        };

        // Capture all by reference: [&]
        auto capture_all_ref = [&]() {
            local = 300;
            value = 400;
            std::cout << "    capture_all_ref: local = " << local
                      << ", this->value = " << value << "\n";
        };

        // Capture this explicitly: [*this] (C++17) copies the object
        auto capture_this_copy = [*this]() mutable {
            std::cout << "    [*this] value = " << value << "\n";
            // value = 500;  // OK inside, but doesn't affect the original
        };

        // Capture this by reference: [this] (C++11) aliases the object
        auto capture_this_ref = [this]() {
            value = 600;  // modifies the actual object
            std::cout << "    [this] value = " << value << "\n";
        };

        by_value();
        by_ref();
        std::cout << "  after by_ref: local = " << local << "\n";
        capture_all_value();
        capture_all_ref();
        std::cout << "  after capture_all_ref: local = " << local
                  << ", value = " << value << "\n";
        capture_this_copy();
        capture_this_ref();
        std::cout << "  after capture_this_ref: value = " << value << "\n";
    }
};

// ============================================================
// 3. Generic lambdas (C++14): auto parameters
// ============================================================

void generic_lambda_demo() {
    // A lambda that works for any type with operator+
    auto add = [](auto a, auto b) {
        return a + b;
    };

    std::cout << "  add(1, 2) = " << add(1, 2) << "\n";
    std::cout << "  add(1.5, 2.5) = " << add(1.5, 2.5) << "\n";
    std::cout << "  add(\"ab\", \"cd\") = " << add(std::string("ab"), std::string("cd")) << "\n";
}

// ============================================================
// 4. Lambda return type deduction vs explicit
// ============================================================

void return_type_demo() {
    // Deduction: return type is deduced from the return statements
    auto deduced = [](int x) {
        if (x > 0) return 3.14;
        return 2.71;  // both return double — deduced as double
    };

    // Explicit: force a specific return type
    auto explicit_ret = [](int x) -> double {
        if (x > 0) return 3;
        return 2;  // ints converted to double
    };

    std::cout << "  deduced(1) = " << deduced(1) << " (type: double)\n";
    std::cout << "  explicit_ret(1) = " << explicit_ret(1) << " (type: double)\n";
}

int main() {
    std::cout << "=== Range-based for loop ===\n";
    range_for_demo();

    std::cout << "\n=== Lambda capture modes ===\n";
    Counter c;
    c.demo();

    std::cout << "\n=== Generic lambdas ===\n";
    generic_lambda_demo();

    std::cout << "\n=== Lambda return types ===\n";
    return_type_demo();

    return 0;
}
