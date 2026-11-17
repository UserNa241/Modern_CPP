// lesson_6_3_cpp17_core.cpp — C++17 core features
//
// ONE idea: C++17 added structured bindings (unpack tuples/arrays/structs),
// inline variables (header-safe globals), std::optional/variant/any (value
// semantics for maybe/wrong/many types), std::string_view (non-owning string
// views), and the filesystem library. Each replaces a common pre-C++17 idiom
// with something safer and clearer.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_6_3_cpp17_core.cpp -o c17 && ./c17

#include <any>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

// ============================================================
// 1. Structured bindings: unpack tuples, arrays, structs
// ============================================================

struct Point {
    int x;
    int y;
};

void structured_bindings_demo() {
    std::pair<int, std::string> p{42, "answer"};
    auto [num, text] = p;                      // unpack pair
    std::cout << "  pair: " << num << " = " << text << "\n";

    int arr[3] = {1, 2, 3};
    auto [a, b, c] = arr;                      // unpack array
    std::cout << "  array: " << a << " " << b << " " << c << "\n";

    Point pt{7, 3};
    auto [px, py] = pt;                        // unpack struct
    std::cout << "  point: (" << px << ", " << py << ")\n";
}

// ============================================================
// 2. Inline variables: header-safe globals (C++17)
// ============================================================

// Before C++17: a non-const global in a header caused ODR violations
// (one definition per .cpp). inline makes it one object across the program.
inline int default_timeout = 30;               // one definition, everywhere
inline const std::string app_name = "demo";    // inline const string

// ============================================================
// 3. std::optional: maybe a value (no dynamic allocation)
// ============================================================

std::optional<int> find_first_positive(const std::vector<int>& v) {
    for (int x : v) {
        if (x > 0) return x;                   // return a value
    }
    return std::nullopt;                       // or return nothing
}

void optional_demo() {
    std::vector<int> v1{-3, -1, 5, -2};
    auto r1 = find_first_positive(v1);
    if (r1) {
        std::cout << "  found positive: " << *r1 << "\n";
    }

    std::vector<int> v2{-5, -7, -9};
    auto r2 = find_first_positive(v2);
    if (!r2) {
        std::cout << "  no positive value (nullopt)\n";
    }

    // Default value when empty
    std::cout << "  r2 value_or(0) = " << r2.value_or(0) << "\n";
}

// ============================================================
// 4. std::variant: one of several types (type-safe union)
// ============================================================

using Token = std::variant<int, double, std::string>;

void variant_demo() {
    Token t = 42;                              // holds int
    std::cout << "  token holds int: " << std::get<int>(t) << "\n";

    t = 3.14;                                  // now holds double
    std::cout << "  token holds double: " << std::get<double>(t) << "\n";


    // std::visit: call the right overload based on active type
    std::visit([](auto&& arg) {
        std::cout << "  visited: " << arg << "\n";
    }, t);

    // Check before get: avoids std::bad_variant_access
    if (std::holds_alternative<std::string>(t)) {
        std::cout << "  token is string\n";
    } else {
        std::cout << "  token is not string\n";
    }
}

// ============================================================
// 5. std::any: one value of any type (type-erased)
// ============================================================

void any_demo() {
    std::any a = 42;                           // holds int
    std::cout << "  any holds int: " << std::any_cast<int>(a) << "\n";


    a = std::string("hello");                  // now holds string
    std::cout << "  any holds string: " << std::any_cast<std::string>(a) << "\n";

    if (a.type() == typeid(std::string)) {
        std::cout << "  any is string type\n";
    }
}

// ============================================================
// 6. std::string_view: non-owning view of a string
// ============================================================

void print_view(std::string_view sv) {
    std::cout << "  view: \"" << sv << "\" (length " << sv.size() << ")\n";
}

void string_view_demo() {
    // string_view points to static storage (string literal) — no lifetime issues
    std::string_view s = "hello world";
    print_view(s);                             // view into static storage
    print_view(std::string_view{"string literal"});  // literal has static storage

    // substring without allocation
    std::string_view sub = s.substr(0, 5);
    std::cout << "  sub: \"" << sub << "\"\n";

    // string_view has contains/starts_with/ends_with (C++20)
    std::cout << "  starts_with \"he\": " << s.starts_with("he") << "\n";
    std::cout << "  contains \"wor\": " << s.contains("wor") << "\n";
    std::cout << "  ends_with \"ld\": " << s.ends_with("ld") << "\n";
}

// ============================================================
// 7. Filesystem library (C++17)
// ============================================================

void filesystem_demo() {
    std::filesystem::create_directories("demo_dir/sub");
    std::filesystem::path p = "demo_dir/sub/test.txt";

    std::ofstream out(p);
    out << "hello from filesystem\n";
    out.close();

    std::cout << "  exists: " << std::filesystem::exists(p) << "\n";
    std::cout << "  filename: " << p.filename() << "\n";
    std::cout << "  extension: " << p.extension() << "\n";

    std::filesystem::remove_all("demo_dir");
}

int main() {
    std::cout << "=== Structured bindings ===\n";
    structured_bindings_demo();

    std::cout << "\n=== Inline variables ===\n";
    std::cout << "  default_timeout = " << default_timeout << "\n";
    std::cout << "  app_name = " << app_name << "\n";

    std::cout << "\n=== std::optional ===\n";
    optional_demo();

    std::cout << "\n=== std::variant ===\n";
    variant_demo();

    std::cout << "\n=== std::any ===\n";
    any_demo();

    std::cout << "\n=== std::string_view ===\n";
    string_view_demo();

    std::cout << "\n=== Filesystem ===\n";
    filesystem_demo();;

    return 0;
}
