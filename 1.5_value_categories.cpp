// lesson_1_5_value_categories.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_1_5_value_categories.cpp -o lesson15 && ./lesson15
#include <iostream>
#include <iomanip>
#include <string>
#include <utility>
#include <type_traits>

// ---------------- the compile-time detector ----------------
// decltype((E)):  lvalue -> T&,   xvalue -> T&&,   prvalue -> T
template <class T> struct value_category      { static constexpr const char* value = "prvalue"; };
template <class T> struct value_category<T&>  { static constexpr const char* value = "lvalue";  };
template <class T> struct value_category<T&&> { static constexpr const char* value = "xvalue";  };

#define SHOW(expr) \
    std::cout << "  " << std::left << std::setw(30) << #expr \
              << " -> " << value_category<decltype((expr))>::value << '\n'

// ---------------- helpers ----------------
int   by_value()          { return 42; }
int&  by_ref(int& inner)  { return inner; }
int&& funnel(int& inner)  { return std::move(inner); }   // returns T&& -> call is xvalue

struct Box { int v; };

const char* which(int&)  { return "int&   (lvalue overload)"; }
const char* which(int&&) { return "int&&  (rvalue overload)"; }

int main() {
    int x = 10, y = 5;
    int* p = &x;
    int& r = x;

    std::cout << "=== 1. The decltype trap: NAME vs EXPRESSION ===\n";
    static_assert(std::is_same_v<decltype( x), int>);   // name  -> declared type
    static_assert(std::is_same_v<decltype((x)), int&>); // (x) is an lvalue expression -> T&
    static_assert(std::is_same_v<decltype((42)), int>); // prvalue -> T
    static_assert(std::is_same_v<decltype((std::move(x))), int&&>); // xvalue -> T&&
    std::cout << "  decltype(x)   = int     (NAME rule: declared type)\n"
              << "  decltype((x)) = int&    (EXPRESSION rule: (x) is an lvalue)\n"
              << "  all four static_asserts passed at COMPILE time\n";

    std::cout << "\n=== 2. Classify a battery of expressions ===\n";
    SHOW(x);                    // lvalue: a name
    SHOW(r);                    // lvalue: using a reference = using its referent
    SHOW(42);                   // prvalue: a literal
    SHOW('h');                  // prvalue
    SHOW("hello");              // lvalue (!!): static array in .rodata, has an address
    SHOW(x + y);                // prvalue: arithmetic result
    SHOW(*p);                   // lvalue: dereference denotes the object
    SHOW(p[0]);                 // lvalue
    SHOW(++x);                  // lvalue: prefix ++ returns the object itself
    SHOW(x++);                  // prvalue: postfix ++ returns a COPY of the old value
    SHOW(std::string("hi"));    // prvalue: T(...) constructs a new value
    SHOW(Box{7});               // prvalue
    SHOW(Box{7}.v);             // xvalue: member of an expiring temporary (C++17)
    SHOW(by_value());           // prvalue: returns T by value
    SHOW(by_ref(x));            // lvalue: returns T&  (existing object)
    SHOW(funnel(x));            // xvalue: returns T&&
    SHOW(std::move(x));         // xvalue: the cast, lvalue -> xvalue
    SHOW(static_cast<int&&>(x)); // xvalue: same thing, spelled manually
    SHOW([]{});                 // prvalue: a lambda IS a value (a closure object)

    std::cout << "\n=== 3. The payoff: overload resolution ===\n";
    std::cout << "  which(x)             -> " << which(x) << '\n';
    std::cout << "  which(42)            -> " << which(42) << '\n';
    std::cout << "  which(std::move(x))  -> " << which(std::move(x)) << '\n';

    std::cout << "\n=== 4. const T& binds EVERYTHING ===\n";
    const int& c1 = x;                  // lvalue         - ok
    const int& c2 = 42;                 // prvalue        - ok (lifetime extended, Lesson 1.6!)
    const int& c3 = std::move(x);       // xvalue         - ok
    std::cout << "  const int& accepted lvalue, prvalue, xvalue: "
              << c1 << ' ' << c2 << ' ' << c3 << '\n';

    std::cout << "\n=== 5. std::move moves NOTHING ===\n";
    int a = 5;
    [[maybe_unused]] auto&& relabeled = std::move(a);    // just an xvalue EXPRESSION bound to a reference
    std::cout << "  after std::move(a), a = " << a
              << "  (untouched! it's a relabel, not an operation)\n"
              << "  the MOVE happens only when an overload is chosen:\n"
              << "    which(std::move(a)) -> " << which(std::move(a)) << '\n';
    return 0;
}
