// Lesson 6.10 — Advanced modern C++ topics
//
// Topics:
//   Deduced `this` — explicit object parameter                          (C++23)
//   Multidimensional subscript — operator[](row, col)                  (C++23)
//   Static operators — static void operator++()                        (C++23, theory-only)
//   Chrono — clocks, time_points, year_month_day, hh_mm_ss             (C++11/20)
//   Error policy — exceptions vs expected vs optional                  (design)
//
// Theory-only (NOT yet in GCC 14.2 libstdc++ — clearly marked):
//   Static operators                                                   (C++23)
//
// Build:
//   g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_6_10_advanced_topics.cpp -o adv && ./adv
//
// One idea: C++23's smaller language refinements plus the chrono library
// give you more expressive types, safer member functions, and better time
// handling — and you must choose your error-handling policy deliberately.

#include <chrono>
#include <iostream>
#include <string>
#include <type_traits>

//===================================================================
// 1. Deduced `this` — explicit object parameter (C++23)
//===================================================================

// Before C++23, you couldn't deduce whether `this` was const, volatile,
// lvalue-ref, or rvalue-ref. You had to overload on const/non-const.
// Now you can write a single template that handles all cases.

struct Widget {
    int value;

    // `this Self&& self` — Self is deduced from the object the method
    // is called on. Works for const, non-const, lvalue, rvalue.
    template <typename Self>
    auto&& get_value(this Self&& self) {
        return self.value;   // preserves value category of self
    }

    // Can also constrain: only allow non-const objects
    template <typename Self>
    void set_value(this Self&& self, int v) requires (!std::is_const_v<std::remove_reference_t<Self>>) {
        self.value = v;
    }
};

//===================================================================
// 2. Multidimensional subscript — operator[](row, col) (C++23)
//===================================================================

// Before C++23, operator[] took exactly one parameter. To index a 2D
// array you had to do m[i][j] (which returns a row proxy). Now operator[]
// can take multiple parameters.
struct Matrix {
    int data[3][3] = {};

    int& operator[](std::size_t row, std::size_t col) {
        return data[row][col];
    }

    const int& operator[](std::size_t row, std::size_t col) const {
        return data[row][col];
    }
};

//===================================================================
// 3. Static operators (C++23) — theory-only, not in GCC 14.2
//===================================================================

/*
   struct Counter {
       static int count;
       static void operator++() { ++count; }   // C++23: static operators
   };
   int Counter::count = 0;

   Allows operator overloading without an object instance. Not yet
   supported in GCC 14.2 — marked theory-only.
*/

//===================================================================
// 4. Chrono — time handling (C++11/20)
//===================================================================

void chrono_demo() {
    using namespace std::chrono;

    // system_clock — wall clock, can be adjusted
    auto now = system_clock::now();
    std::cout << "  system_clock now: "
              << time_point_cast<milliseconds>(now).time_since_epoch().count()
              << " ms since epoch\n";

    // steady_clock — monotonic, never adjusted — use for measuring intervals
    auto start = steady_clock::now();
    // ... some work ...
    auto end = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(end - start);
    std::cout << "  steady_clock elapsed: " << elapsed.count() << " ms\n";

    // year_month_day — calendar date, type-safe
    auto today = floor<days>(system_clock::now());
    year_month_day ymd{today};
    std::cout << "  today: " << int(ymd.year()) << "/"
              << unsigned(ymd.month()) << "/"
              << unsigned(ymd.day()) << "\n";

    // hh_mm_ss — time of day
    auto tod = hh_mm_ss{today.time_since_epoch()};
    std::cout << "  time: " << tod.hours().count() << ":"
              << tod.minutes().count() << ":"
              << tod.seconds().count() << "\n";

    // duration literals
    using namespace std::chrono_literals;
    auto timeout = 500ms;
    auto period  = 1h + 30min;
    std::cout << "  timeout: " << timeout.count() << " ms\n";
    std::cout << "  period:  " << period.count() << " s\n";
}

//===================================================================
// helpers
//===================================================================

static void show(const char* label, auto v) {
    std::cout << "  " << label << ": " << v << "\n";
}

int main() {
    //---------------------------------------------------------------
    // Deduced `this`
    //---------------------------------------------------------------
    std::cout << "=== Deduced `this` ===\n";
    Widget w{42};
    show("w.get_value()", w.get_value());
    w.set_value(99);
    show("after set_value(99)", w.get_value());

    // Call on an rvalue — Self deduced as Widget&&
    show("Widget{7}.get_value()", Widget{7}.get_value());

    //---------------------------------------------------------------
    // Multidimensional subscript
    //---------------------------------------------------------------
    std::cout << "=== Multidimensional subscript ===\n";
    Matrix m;
    m[1, 2] = 99;          // one operator[] call, two indices
    show("m[1, 2]", m[1, 2]);

    const Matrix& cm = m;
    show("cm[1, 2] (const)", cm[1, 2]);

    //---------------------------------------------------------------
    // Chrono
    //---------------------------------------------------------------
    std::cout << "=== Chrono ===\n";
    chrono_demo();

    //---------------------------------------------------------------
    // Error policy — design discussion
    //---------------------------------------------------------------
    std::cout << "=== Error policy ===\n";
    std::cout << "  Use exceptions for: programmer errors, unrecoverable failures\n";
    std::cout << "  Use std::expected for: recoverable errors with diagnostic info\n";
    std::cout << "  Use std::optional for: maybe-a-value, no reason needed\n";
    std::cout << "  Pick one policy per codebase and apply consistently\n";

    return 0;
}
