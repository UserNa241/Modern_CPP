// Lesson 6.5 — C++20 core features
//
// Topics (all from the §6 roadmap title):
//   operator<=> (spaceship) + = default comparison      (C++20)
//   designated initializers                              (C++20)
//   std::span — non-owning view over contiguous data     (C++20)
//   std::jthread — thread that joins on destruction      (C++20)
//   atomic wait / notify                                 (C++20)
//   char8_t — distinct UTF-8 character type              (C++20)
//   std::source_location — caller file/line in params    (C++20)
//   std::is_constant_evaluated() — compile-time vs runtime (C++20)
//
// Build:
//   g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_6_5_cpp20_core.cpp -o c20 && ./c20
//
// One idea: C++20 core features replace C-style idioms with type-safe,
// bounds-aware, self-documenting alternatives.

#include <atomic>       // std::atomic, wait/notify
#include <compare>      // std::strong_ordering, operator<=>
#include <cstdint>      // std::uintptr_t
#include <iostream>
#include <span>         // std::span
#include <source_location>
#include <string>
#include <string_view>
#include <thread>       // std::jthread
#include <type_traits>  // std::is_constant_evaluated
#include <vector>

//===================================================================
// 1. operator<=> (spaceship) + = default — auto-generate comparisons
//===================================================================

struct Point {
    int x;
    int y;

    // One declaration generates ==, !=, <, <=, >, >=.
    auto operator<=>(const Point&) const = default;
};

//===================================================================
// 2. Designated initializers — name the fields you initialize
//===================================================================

// Point p{.x = 5, .y = 7};   // C++20 named-field init (used in main)

//===================================================================
// 3. std::span — non-owning view: pointer + length together
//===================================================================

double sum_of_squares(std::span<const int> data) {
    double total = 0.0;
    for (int v : data) {
        total += static_cast<double>(v) * static_cast<double>(v);
    }
    return total;
}

//===================================================================
// 4. std::source_location — caller's file/line, auto-filled
//===================================================================

void log(std::string_view message,
         const std::source_location loc = std::source_location::current()) {
    std::cout << "  [" << loc.file_name() << ":" << loc.line() << "] "
              << message << "\n";
}

//===================================================================
// 5. std::is_constant_evaluated() — compile-time vs runtime branch
//===================================================================

constexpr int compute(int v) {
    if (std::is_constant_evaluated()) {
        return v * 2;            // compile-time path
    }
    return v + 2;                // runtime path
}

//===================================================================
// helpers
//===================================================================

static void show(const char* label, auto v) {
    std::cout << "  " << label << ": " << v << "\n";
}

int main() {
    //---------------------------------------------------------------
    // 1. operator<=> (spaceship)
    //---------------------------------------------------------------
    std::cout << "=== operator<=> (spaceship) ===\n";
    Point a{1, 2};
    Point b{1, 3};
    Point c{1, 2};

    show("a <=> b is less", a <=> b == std::strong_ordering::less);
    show("a == c", a == c);      // compiler-generated ==
    show("a < b", a < b);        // compiler-generated <
    show("b > a", b > a);        // compiler-generated >

    //---------------------------------------------------------------
    // 2. designated initializers
    //---------------------------------------------------------------
    std::cout << "=== designated initializers ===\n";
    Point p{.x = 5, .y = 7};     // name the fields explicitly
    show("p.x", p.x);
    show("p.y", p.y);

    //---------------------------------------------------------------
    // 3. std::span — vector, subrange, and C-array
    //---------------------------------------------------------------
    std::cout << "=== std::span ===\n";
    std::vector<int> v = {1, 2, 3, 4, 5};
    show("sum_of_squares(full vector)", sum_of_squares(v));

    std::span<const int> sub(v.data() + 1, 3);   // {2, 3, 4}, no copy
    show("sum_of_squares(sub[1..3])", sum_of_squares(sub));

    int arr[] = {10, 20, 30};
    show("sum_of_squares(c-array)", sum_of_squares(arr));

    //---------------------------------------------------------------
    // 4. std::source_location
    //---------------------------------------------------------------
    std::cout << "=== std::source_location ===\n";
    log("program started");          // caller location auto-filled
    log("processing item 42");       // different line

    //---------------------------------------------------------------
    // 5. std::is_constant_evaluated()
    //---------------------------------------------------------------
    std::cout << "=== std::is_constant_evaluated() ===\n";
    constexpr int ct = compute(5);   // compile-time: 5 * 2 = 10
    int rt = compute(5);             // runtime:    5 + 2 = 7
    show("constexpr compute(5)", ct);
    show("runtime compute(5)", rt);

    //---------------------------------------------------------------
    // 6. char8_t — distinct UTF-8 character type
    //---------------------------------------------------------------
    std::cout << "=== char8_t ===\n";
    char8_t ch = u8'A';
    show("char8_t 'A'", static_cast<int>(ch));
    // char8_t does not implicitly convert to char or int — use static_cast

    //---------------------------------------------------------------
    // 7. std::jthread + 8. atomic wait / notify
    //---------------------------------------------------------------
    std::cout << "=== std::jthread + atomic wait/notify ===\n";
    std::atomic<int> flag{0};

    // jthread joins automatically when it goes out of scope (no detach needed)
    std::jthread worker([&flag] {
        flag.store(1);
        flag.notify_one();   // wake up any thread waiting on flag
    });

    flag.wait(0);            // block until flag changes from 0
    show("flag after wait/notify", flag.load());

    return 0;
}
