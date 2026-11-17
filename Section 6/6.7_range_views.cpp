// Lesson 6.7 — Ranges & views deep dive
//
// Topics:
//   Lazy pipelines: filter / transform / take / drop                  (C++20)
//   views::zip / views::enumerate                                     (C++20 / C++23)
//   views::chunk / views::slide / views::adjacent                     (C++23)
//   views::join                                                       (C++20)
//   ranges::to — materialize a view into a container                  (C++23)
//
// Theory-only (NOT yet in GCC 14.2 libstdc++ — clearly marked):
//   std::ranges::fold_left / std::ranges::fold_right                  (C++23)
//
// Build:
//   g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_6_7_ranges_views.cpp -o ranges && ./ranges
//
// One idea: ranges let you express data processing as lazy, composable
// pipelines instead of writing nested loops and temporary containers.

#include <algorithm>
#include <iostream>
#include <ranges>
#include <vector>
#include <string>
#include <numeric>

//===================================================================
// helpers
//===================================================================

template<typename R>
static void print_range(const char* label, R&& r) {
    std::cout << label << ": ";
    for (auto&& x : r) {
        std::cout << x << " ";
    }
    std::cout << "\n";
}

int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<std::string> names = {"ada", "bob", "eva"};

    //---------------------------------------------------------------
    // Lazy pipelines: filter + transform (composable, no temporaries)
    //---------------------------------------------------------------
    std::cout << "=== lazy pipeline: filter + transform ===\n";
    auto pipeline = nums
        | std::views::filter([](int n) { return n % 2 == 0; })   // keep evens
        | std::views::transform([](int n) { return n * 10; });   // x10
    print_range("evens x10", pipeline);
    // Nothing runs until you iterate. No intermediate vector is built.

    //---------------------------------------------------------------
    // views::zip — pair elements from two ranges
    //---------------------------------------------------------------
    std::cout << "=== views::zip ===\n";
    for (auto [name, n] : std::views::zip(names, nums)) {
        std::cout << "  " << name << " = " << n << "\n";
    }
    // Stops at the shortest range. zip is lazy — no tuple vector built.

    //---------------------------------------------------------------
    // views::enumerate — index + value
    //---------------------------------------------------------------
    std::cout << "=== views::enumerate ===\n";
    for (auto [i, name] : std::views::enumerate(names)) {
        std::cout << "  [" << i << "] " << name << "\n";
    }
    // Replaces manual int i = 0; i++ loops.

    //---------------------------------------------------------------
    // views::chunk — split into fixed-size blocks
    //---------------------------------------------------------------
    std::cout << "=== views::chunk(3) ===\n";
    for (auto chunk : nums | std::views::chunk(3)) {
        std::cout << "  chunk: ";
        for (int x : chunk) std::cout << x << " ";
        std::cout << "\n";
    }
    // Last chunk may be smaller (10 elements / 3 = 3,3,3,1).

    //---------------------------------------------------------------
    // views::slide — overlapping windows
    //---------------------------------------------------------------
    std::cout << "=== views::slide(3) ===\n";
    for (auto window : nums | std::views::slide(3)) {
        std::cout << "  window: ";
        for (int x : window) std::cout << x << " ";
        std::cout << "\n";
    }
    // {1,2,3}, {2,3,4}, {3,4,5}, ... — useful for moving averages.

    //---------------------------------------------------------------
    // views::adjacent — non-overlapping pairs/triples
    //---------------------------------------------------------------
    std::cout << "=== views::adjacent<2> ===\n";
    for (auto [a, b] : nums | std::views::adjacent<2>) {
        std::cout << "  (" << a << "," << b << ") ";
    }
    std::cout << "\n";
    // {1,2}, {2,3}, {3,4}, ... — each element appears in two pairs.

    //---------------------------------------------------------------
    // views::join — flatten a range of ranges
    //---------------------------------------------------------------
    std::cout << "=== views::join ===\n";
    std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}, {5, 6}};
    print_range("joined", nested | std::views::join);
    // 1 2 3 4 5 6 — one flat sequence, lazily.

    //---------------------------------------------------------------
    // ranges::to — materialize a view into a container
    //---------------------------------------------------------------
    std::cout << "=== ranges::to<std::vector> ===\n";
    auto result = nums
        | std::views::filter([](int n) { return n > 5; })
        | std::ranges::to<std::vector>();   // C++23: build container directly
    print_range("filtered->vector", result);
    // Before C++23 you needed: copy into std::back_inserter(vec).

    //---------------------------------------------------------------
    // Theory-only: fold_left / fold_right (NOT in GCC 14.2)
    //---------------------------------------------------------------
    std::cout << "=== theory-only: std::ranges::fold_left / fold_right ===\n";
    std::cout << std::ranges::fold_left(nums, 0, std::plus<>{}) << "    left-associative reduce\n";
    std::cout << std::ranges::fold_right(nums, 0, std::plus<>{}) << "   right-associative reduce\n";
    // Equivalent today: std::accumulate(v.begin(), v.end(), 0, std::plus<>{});

    return 0;
}
