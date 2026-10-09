// lesson_4_3_perfect_forwarding.cpp — std::forward & perfect forwarding.
//
// ONE idea: an intermediary (factory, emplace, wrapper) wants to pass an
// argument through to a real constructor PRESERVING its value category.
//   const T& accepts everything but SHREDS the category (everything copies).
//   T&& + std::forward<T> accepts everything and RESTORES the category at
//   the last moment, because the category was stored in T at deduction (4.2).
//
//   1. the acid test: parity table — direct call vs const& factory vs
//      perfect factory, for an lvalue and an rvalue
//   2. the conditional cast: std::forward, read through the collapsing rules
//   3. the exclusion: move-only arguments (const& factory cannot even compile)
//   4. the §3 line, explained: make_box(A&&...) — the pattern inside every
//      make_shared/make_unique/emplace_back we have used since Section 2
//
// Build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_4_3_perfect_forwarding.cpp -o pf && ./pf
// MSVC : cl /std:c++20 /EHsc /W4 lesson_4_3_perfect_forwarding.cpp

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------- the instrumented payload
struct Probe {
    static inline int copies = 0, moves = 0;
    static void reset() { copies = moves = 0; }
    Probe() = default;
    Probe(const Probe&) { ++copies; }
    Probe(Probe&&) noexcept { ++moves; }
};

// ---------------------------------------- the two factories
// BAD (1998): accepts everything, erases the category. The parameter's TYPE
// is the same for lvalues and rvalues — the information dies at the binding.
template <class A>
Probe make_bad(const A& a) {
    return Probe(a);          // 'a' is just a const lvalue in here: always copies
}

// GOOD (2011): forwarding reference + forward. The category was stored in A
// at deduction (4.2's matrix); forward<A> consults A and restores it HERE.
template <class A>
Probe make_good(A&& a) {
    return Probe(std::forward<A>(a));
}

void row(const char* what, int c, int m) {
    // (a hand-rolled pad like 26 - strlen(what) UNDERFLOWS for long labels —
    //  size_t wraps, and the string ctor asks for gigabytes. setw is safe.)
    std::cout << "    " << std::left << std::setw(30) << what
              << "copies=" << c << " moves=" << m << "\n";
}

void demo_parity() {
    std::cout << "=== 1. The acid test: parity with a DIRECT call ===\n";
    std::cout << "    (an intermediary is PERFECT if the same constructor fires\n";
    std::cout << "     that a direct call would have fired)\n";
    Probe src;
    std::cout << "  lvalue argument:\n";
    Probe::reset();
    Probe d1 = src;                      row("direct: Probe d = src", Probe::copies, Probe::moves);
    Probe::reset();
    Probe b1 = make_bad(src);            row("const& factory", Probe::copies, Probe::moves);
    Probe::reset();
    Probe g1 = make_good(src);           row("perfect factory", Probe::copies, Probe::moves);
    std::cout << "  rvalue argument:\n";
    Probe::reset();
    Probe d2 = std::move(src);           row("direct: Probe d = move(src)", Probe::copies, Probe::moves);
    Probe src2;
    Probe::reset();
    Probe b2 = make_bad(std::move(src2)); row("const& factory", Probe::copies, Probe::moves);
    Probe src3;
    Probe::reset();
    Probe g2 = make_good(std::move(src3)); row("perfect factory", Probe::copies, Probe::moves);
    std::cout << "  -> lvalue row: all three copy (parity). rvalue row: const&\n";
    std::cout << "     factory COPIES where direct moved — category shredded.\n";
    std::cout << "     Perfect factory matches direct on BOTH rows. That is the claim.\n";
    (void)d1; (void)b1; (void)g1; (void)d2; (void)b2; (void)g2;
}

// ---------------------------------------- 2. the conditional cast itself
// std::forward's essential half, in full:
template <class T>
constexpr T&& my_forward(std::remove_reference_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}

void demo_conditional() {
    std::cout << "\n=== 2. The conditional cast: one static_cast, two behaviors ===\n";
    std::cout << "    template <class T> T&& forward(remove_reference_t<T>& t)\n";
    std::cout << "                               { return static_cast<T&&>(t); }\n";
    int x = 1;
    static_assert(std::is_same_v<decltype(my_forward<int>(x)), int&&>);   // T=int: relabel to rvalue
    static_assert(std::is_same_v<decltype(my_forward<int&>(x)), int&>);   // T=int&: int& && -> int&
    std::cout << "    A = int   (rvalue arrived): static_cast<int&&>   -> MOVE permitted\n";
    std::cout << "    A = int&  (lvalue arrived): static_cast<int& &&> -> collapses -> identity\n";
    std::cout << "    The branch is chosen at DEDUCTION — zero runtime cost, and\n";
    std::cout << "    std::move is just the T=int half, hardcoded (4.2's proof).\n";
    std::cout << "    (forward WITHOUT its T — std::forward(x) — cannot compile: it\n";
    std::cout << "     has nothing to consult. The category lives in T, never in x.)\n";
}

// ---------------------------------------- 3. the move-only exclusion
struct Own {
    std::unique_ptr<int> payload;
    explicit Own(std::unique_ptr<int> p) : payload(std::move(p)) {}
};

template <class A>
Own make_own_bad(const A& a) {           // would copy: OWN has no copy —
    return Own(a);                       // compile error, commented at the call
}
template <class A>
Own make_own(A&& a) {
    return Own(std::forward<A>(a));      // move for rvalues, identity for lvalues —
}                                        // and lvalue Own arguments fail to compile
                                         // inside Own's ctor (honest failure at the
                                         // RIGHT layer, with the RIGHT message)

void demo_exclusion() {
    std::cout << "\n=== 3. The exclusion: move-only arguments ===\n";
    auto p = std::make_unique<int>(42);
    Own o = make_own(std::move(p));      // forwarded as rvalue: unique_ptr moved in
    std::cout << "    make_own(std::move(p)) -> payload = " << *o.payload
              << ", p.get() = " << p.get() << " (moved from, not copied —\n";
    std::cout << "    copying was never even an option)\n";
    std::cout << "    // make_own_bad(std::move(p2));  // compile error: Own(const Own&)\n";
    std::cout << "    //                              // is deleted — const& factory\n";
    std::cout << "    //                              // cannot express 'take it'\n";
}

// ---------------------------------------- 4. the §3 line, explained
struct Box {
    Probe a;
    std::string tag;
    static inline int born = 0;
    Box(Probe pa, std::string t) : a(std::move(pa)), tag(std::move(t)) { ++born; }
};

// The pattern from 3.5/3.4 verbatim (this is my::make_shared's shape):
// template <class T, class... A>
// shared_ptr<T> make_shared(A&&... a) {
//     auto* blk = new CountedInPlace<T>(std::forward<A>(a)...);   // <- THE LINE
//     ...
// }
template <class... A>
Box make_box(A&&... a) {
    return Box(std::forward<A>(a)...);   // pack expansion: EACH argument is
}                                        // forwarded with ITS OWN deduced A

void demo_section3_line() {
    std::cout << "\n=== 4. The line inside every factory since Section 2 ===\n";
    Probe src;
    std::string name = "box";
    Probe::reset();
    auto b1 = make_box(src, name);       // lvalue, lvalue
    std::cout << "    make_box(src, name)          copies=" << Probe::copies
              << " moves=" << Probe::moves << "  (both copied: parity with direct)\n";
    Probe::reset();
    auto b2 = make_box(Probe{}, "temp"); // rvalue, rvalue
    std::cout << "    make_box(Probe{}, \"temp\")    copies=" << Probe::copies
              << " moves=" << Probe::moves << "  (zero copies — Constraint B)\n";
    Probe::reset();
    auto b3 = make_box(src, std::move(name));   // MIXED: one of each
    std::cout << "    make_box(src, move(name))    copies=" << Probe::copies
              << " moves=" << Probe::moves << "  (each arg handled by ITS OWN A)\n";
    std::cout << "    -> std::forward<A>(a)... expands to one forward PER argument,\n";
    std::cout << "       each consulting its own deduced A. One template, N arguments,\n";
    std::cout << "       no 2^N overloads, no lost categories. This is why emplace_back,\n";
    std::cout << "       make_unique and every A&&... factory we wrote just works.\n";
    (void)b1; (void)b2; (void)b3;
}

int main() {
    demo_parity();
    demo_conditional();
    demo_exclusion();
    demo_section3_line();
    return 0;
}
