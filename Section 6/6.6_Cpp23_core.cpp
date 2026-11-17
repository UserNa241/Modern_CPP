// Lesson 6.6 — C++23 core features (everyday development focus)
//
// Topics (per §6 roadmap):
//   std::expected + monadic ops (and_then / or_else / transform / transform_error)  (C++23)
//   std::print / std::println                                                         (C++23)
//   std::byteswap                                                                     (C++23)
//   std::to_underlying                                                                (C++23)
//   std::unreachable                                                                  (C++23)
//   if consteval                                                                      (C++23)
//
// Theory-only (NOT yet in GCC 14.2 libstdc++ — clearly marked):
//   std::flat_map / std::flat_set                                                     (C++23)
//   std::mdspan                                                                       (C++23)
//
// Paired constraint §6-B: std::expected monadic chain + std::print/format for errors.
// Exceptions are banned for recoverable paths; and_then is required.
//
// Build:
//   g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_6_6_cpp23_core.cpp -o c23 && ./c23
//
// One idea: C++23 gives you exception-free error handling (expected), modern text
// output (print), and small utility features for code you write every day.

#include <bit>        // std::byteswap
#include <expected>   // std::expected
#include <iostream>
#include <print>      // std::print / std::println
#include <string>
#include <string_view>
#include <utility>    // std::to_underlying, std::unreachable
#include <vector>

//===================================================================
// 1. std::expected — a value or an error, without exceptions
//===================================================================

// Everyday example: parsing a user's age input. "Failure" (not a number,
// out of range) is normal, not exceptional — so we use expected, not throw.
struct ParseError {
    std::string message;
};

std::expected<int, ParseError> parse_age(std::string_view input) {
    if (input.empty()) {
        return std::unexpected(ParseError{"empty input"});
    }
    int value = 0;
    for (char c : input) {
        if (c < '0' || c > '9') {
            return std::unexpected(ParseError{"non-digit"});
        }
        value = value * 10 + (c - '0');
    }
    if (value < 0 || value > 150) {
        return std::unexpected(ParseError{"out of range"});
    }
    return value;
}

//===================================================================
// 2. Monadic ops: and_then / or_else / transform / transform_error
//===================================================================

// and_then: chain operations that can each fail. Stop at first error.
std::expected<double, ParseError> age_to_months(std::expected<int, ParseError> age) {
    return age.and_then([](int years) -> std::expected<double, ParseError> {
        if (years == 0) {
            return std::unexpected(ParseError{"age zero"});
        }
        return static_cast<double>(years) * 12.0;
    });
}

// or_else: provide a fallback when an operation fails.
std::expected<int, ParseError> with_default(std::expected<int, ParseError> age) {
    return age.or_else([](const ParseError&) {
        return std::expected<int, ParseError>{18};   // must return expected
    });
}

// transform: apply a function to the success value, keep error type.
std::expected<std::string, ParseError> age_message(std::expected<int, ParseError> age) {
    return age.transform([](int years) {
        return "Age is " + std::to_string(years);
    });
}

// transform_error: change the error representation without touching success.
std::expected<int, std::string> repackage_error(std::expected<int, ParseError> age) {
    return age.transform_error([](const ParseError& e) {
        return e.message;
    });
}

//===================================================================
// 3. std::print / std::println — modern text output
//===================================================================

void print_demo() {
    std::println("Hello, C++23!");              // println adds newline
    std::print("Name: {}, Age: {}\n", "Ada", 36); // format-style, no endl needed
    std::print("Pi ≈ {:.2f}\n", 3.14159);
}

//===================================================================
// 4. std::byteswap — swap byte order of an integer
//===================================================================

void byteswap_demo() {
    std::uint16_t net = 0x1234;
    std::uint16_t host = std::byteswap(net);
    std::println("byteswap(0x{:04x}) = 0x{:04x}", net, host);
}

//===================================================================
// 5. std::to_underlying — get the integer value of an enum
//===================================================================

enum class Color : unsigned char { Red, Green, Blue };

void to_underlying_demo() {
    Color c = Color::Green;
    std::println("Color::Green = {}", std::to_underlying(c));
    // Before C++23 you needed static_cast<unsigned>(c)
}

//===================================================================
// 6. std::unreachable — tell the compiler a path can't be reached
//===================================================================

int classify(int score) {
    if (score < 0) return -1;
    if (score > 100) {
        std::unreachable();   // optimization hint: this can't happen
    }
    if (score >= 90) return 4;
    if (score >= 80) return 3;
    if (score >= 70) return 2;
    if (score >= 60) return 1;
    return 0;
}

//===================================================================
// 7. if consteval — branch between compile-time and runtime
//===================================================================

constexpr int compute(int v) {
    if consteval {
        return v * 2;   // compile-time path
    }
    return v + 2;       // runtime path
}

//===================================================================
// Theory-only (NOT in GCC 14.2 libstdc++): flat_map, flat_set, mdspan
//===================================================================

// std::flat_map / std::flat_set — sorted vector-backed map/set.
//   Contiguous storage, cache-friendly, but slower insertions (shifting).
//   Syntax: std::flat_map<std::string, int> m;  m["a"] = 1;
// std::mdspan — multidimensional non-owning view (like span for 2D+ arrays).
//   Syntax: std::mdspan<double, std::extents<3, 4>> m(ptr);
//   Both are C++23 but not yet shipped in libstdc++ as of GCC 14.2.

//===================================================================
// helpers
//===================================================================

static void show(const char* label, auto v) {
    std::println("  {}: {}", label, v);
}

int main() {
    //---------------------------------------------------------------
    // std::expected — value or error, no exceptions
    //---------------------------------------------------------------
    std::println("=== std::expected ===");
    auto r1 = parse_age("25");
    if (r1) {
        std::println("  parsed age: {}", *r1);
    } else {
        std::println("  error: {}", r1.error().message);
    }

    auto r2 = parse_age("abc");
    if (!r2) {
        std::println("  error: {}", r2.error().message);
    }

    //---------------------------------------------------------------
    // Monadic chain — and_then (paired constraint §6-B)
    //---------------------------------------------------------------
    std::println("=== monadic chain (and_then) ===");
    auto months = age_to_months(parse_age("5"));
    if (months) {
        std::println("  months: {}", *months);
    } else {
        std::println("  chain error: {}", months.error().message);
    }

    auto bad_months = age_to_months(parse_age("abc"));
    if (!bad_months) {
        std::println("  chain error propagates: {}", bad_months.error().message);
    }

    //---------------------------------------------------------------
    // or_else — fallback default
    //---------------------------------------------------------------
    std::println("=== or_else ===");
    auto fallback = with_default(parse_age("abc"));
    std::println("  fallback age: {}", *fallback);

    //---------------------------------------------------------------
    // transform — map success value
    //---------------------------------------------------------------
    std::println("=== transform ===");
    auto msg = age_message(parse_age("42"));
    if (msg) {
        std::println("  {}", *msg);
    }

    //---------------------------------------------------------------
    // transform_error — repackage error
    //---------------------------------------------------------------
    std::println("=== transform_error ===");
    auto rep = repackage_error(parse_age("abc"));
    if (!rep) {
        std::println("  repackaged error: {}", rep.error());
    }

    //---------------------------------------------------------------
    // std::print / println
    //---------------------------------------------------------------
    std::println("=== std::print / println ===");
    print_demo();

    //---------------------------------------------------------------
    // std::byteswap
    //---------------------------------------------------------------
    std::println("=== std::byteswap ===");
    byteswap_demo();

    //---------------------------------------------------------------
    // std::to_underlying
    //---------------------------------------------------------------
    std::println("=== std::to_underlying ===");
    to_underlying_demo();

    //---------------------------------------------------------------
    // std::unreachable
    //---------------------------------------------------------------
    std::println("=== std::unreachable ===");
    std::println("  classify(85) = {}", classify(85));

    //---------------------------------------------------------------
    // if consteval
    //---------------------------------------------------------------
    std::println("=== if consteval ===");
    constexpr int ct = compute(5);   // compile-time: 10
    int rt = compute(5);             // runtime:   7
    show("constexpr compute(5)", ct);
    show("runtime compute(5)", rt);

    //---------------------------------------------------------------
    // Theory-only note
    //---------------------------------------------------------------
    std::println("=== theory-only (not in GCC 14.2 libstdc++) ===");
    std::println("  flat_map / flat_set: sorted vector-backed containers");
    std::println("  mdspan: multidimensional non-owning array view");

    return 0;
}
