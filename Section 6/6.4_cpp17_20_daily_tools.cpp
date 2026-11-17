// Lesson 6.4 — C++17/20 daily tools
//
// Topics:
//   string::contains / starts_with / ends_with   (C++20 / C++23)
//   if / switch with initializer                  (C++17)
//   std::byte                                     (C++17)
//   digit separators                              (C++14)
//   override / final                              (C++11)
//   default / delete                              (C++11)
//
// Build:
//   g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_6_4_daily_tools.cpp -o daily && ./daily
//
// One idea: small, well-scoped language features that make everyday C++ code
// clearer, safer, and less error-prone.

#include <cstddef>   // std::byte
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

//===================================================================
// override / final — virtual-function specifiers
//===================================================================

struct Base {
    virtual void draw() const { std::cout << "  Base::draw\n"; }
    virtual ~Base() = default;
};

struct Derived : Base {
    // 'override' guarantees this really overrides a base virtual;
    // the compiler errors if the signature doesn't match.
    void draw() const override { std::cout << "  Derived::draw\n"; }
};

struct Sealed : Base {
    // 'final' on a function: no further class can override draw().
    void draw() const final { std::cout << "  Sealed::draw\n"; }
};

// class NoMore : public Sealed {   // ERROR: cannot derive from 'Sealed'
//     void draw() const override { ... }
// };

//===================================================================
// default / delete — explicitly request or forbid special members
//===================================================================

struct Widget {
    Widget() = default;               // explicitly ask for the default ctor
    Widget(const Widget&) = delete;   // forbid copying
    Widget& operator=(const Widget&) = delete;
    Widget(Widget&&) = default;       // allow moving
    ~Widget() = default;
};

//===================================================================
// helpers
//===================================================================

static void show(const std::string& label, bool v) {
    std::cout << "  " << label << ": " << (v ? "true" : "false") << "\n";
}

int main() {
    //---------------------------------------------------------------
    // string::contains / starts_with / ends_with (C++20 / C++23)
    //---------------------------------------------------------------
    std::cout << "=== string::contains / starts_with / ends_with ===\n";
    std::string text = "C++23 daily tools";
    show("contains \"daily\"", text.contains("daily"));
    show("starts_with \"C++\"", text.starts_with("C++"));
    show("ends_with \"tools\"", text.ends_with("tools"));

    std::string_view sv = text;
    show("sv contains \"23\"", sv.contains("23"));
    // Old idiom: text.find("daily") != std::string::npos

    //---------------------------------------------------------------
    // if / switch with initializer (C++17)
    //---------------------------------------------------------------
    std::cout << "=== if / switch with initializer ===\n";
    if (std::vector<int> v{1, 2, 3}; !v.empty()) {
        // 'v' lives only inside this if-block
        std::cout << "  vector not empty, size = " << v.size() << "\n";
    }
    // 'v' is not accessible here

    int code = 2;
    switch (auto status = code * 10; status) {
        case 10: std::cout << "  status 10\n"; break;
        case 20: std::cout << "  status 20\n"; break;
        default: std::cout << "  other status\n";
    }
    // 'status' lives only inside this switch

    //---------------------------------------------------------------
    // digit separators (C++14)
    //---------------------------------------------------------------
    std::cout << "=== digit separators ===\n";
    long long big = 1'000'000'000LL;
    double pi = 3.141'592'653'589'793;
    std::cout << "  big = " << big << ", pi = " << pi << "\n";
    std::cout << "  hex mask = 0xFF'FF'00'00\n";
    std::cout << "  binary = 0b1010'1111\n";

    //---------------------------------------------------------------
    // std::byte (C++17)
    //---------------------------------------------------------------
    std::cout << "=== std::byte ===\n";
    std::byte b = std::byte{0x42};
    std::cout << "  byte value = " << static_cast<unsigned>(b) << "\n";
    std::byte b2 = b | std::byte{0x01};      // bitwise ops defined on byte
    std::cout << "  byte OR 0x01 = " << static_cast<unsigned>(b2) << "\n";
    // int x = b;  // ERROR: no implicit conversion from byte to integer

    //---------------------------------------------------------------
    // override / final at runtime
    //---------------------------------------------------------------
    std::cout << "=== override / final ===\n";
    Base* p = new Derived();
    p->draw();      // prints Derived::draw (virtual dispatch)
    delete p;

    Sealed s;
    s.draw();       // prints Sealed::draw (final: nothing can override it)

    //---------------------------------------------------------------
    // default / delete at runtime
    //---------------------------------------------------------------
    std::cout << "=== default / delete ===\n";
    Widget w;        // default ctor OK
    Widget w2 = std::move(w);   // move OK
    // Widget w3 = w;   // ERROR: copy constructor is deleted

    std::cout << "  Widget default + move constructed OK\n";
    return 0;
}
