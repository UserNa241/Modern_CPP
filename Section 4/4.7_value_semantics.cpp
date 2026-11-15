// lesson_4_7_value_semantics.cpp — value vs reference semantics, the sink idiom
// measured, and auto(x) (C++23).
//
// ONE idea: semantics is a choice you make ON PURPOSE. Value = independence and
// local reasoning; reference = shared state and global reasoning. The parameter-
// passing decision table makes the choice mechanical; the sink idiom is its most
// used row; auto(x) is decay-copy as a composable expression.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_4_7_value_semantics.cpp -o vs && ./vs
// MSVC : cl /std:c++latest /EHsc /W4 lesson_4_7_value_semantics.cpp
//        (auto(x) IS C++23: without it GCC says "auto(x) only available with -std=c++2b")
//
// Machine-verified claims in this file (probed before writing, ASan as referee):
//   auto(x) decays references (int& -> int) and arrays (int[5] -> int*)
//   auto(x) does NOT unwrap proxy classes: auto(bits[0]) is vector<bool>::reference
//   reading a decay-copied proxy after its owner died = heap-use-after-free (ASan-confirmed)

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------- A. the two semantics, face to face ----------------
void demo_semantics() {
    std::cout << "=== A. the two semantics, face to face ===\n";
    std::string a = "hello";
    std::string b = a;                 // VALUE: b is an independent object
    b += " world";
    std::cout << "  value    : b += \" world\" -> a == \"" << a << "\"  (b's change stayed in b)\n";
    std::string& r = a;                // REFERENCE: r is a second NAME for a
    r += "!";
    std::cout << "  reference: r += \"!\"      -> a == \"" << a << "\"  (mutated via an alias)\n";
    std::cout << "  to reason about 'a' you must now know every alias ever handed out:\n";
    std::cout << "  that is reference semantics' tax; independence is value semantics' gift\n";
}

// ---------------- B. the sink idiom, measured ----------------
struct Blob {
    static inline int copies = 0, moves = 0;
    static void reset() { copies = moves = 0; }
    std::string payload;
    explicit Blob(std::size_t n) : payload(n, 'x') {}
    Blob(const Blob& o) : payload(o.payload) { ++copies; }
    Blob(Blob&& o) noexcept : payload(std::move(o.payload)) { ++moves; }
    Blob& operator=(const Blob& o) { payload = o.payload; ++copies; return *this; }
    Blob& operator=(Blob&& o) noexcept { payload = std::move(o.payload); ++moves; return *this; }
};

struct Recorder {
    Blob held{0};
    void set_const_ref(const Blob& b) { held = b; }               // reader: category erased at binding (4.3)
    void set_sink(Blob b) { held = std::move(b); }                // sink: copy OR move at the boundary
    void set_pair(const Blob& b) { held = b; }                    // the overload PAIR (optimal, 2 bodies)
    void set_pair(Blob&& b) { held = std::move(b); }
    void maybe_const(const Blob& b, bool store) { if (store) held = b; }     // reader: free when not storing
    void maybe_sink(Blob b, bool store) { if (store) held = std::move(b); }  // sink: paid even when not
};

template <class F>
void row(const char* name, F f) {
    Blob src(64);
    Blob::reset(); f(src);
    std::cout << "  " << name << " lvalue: " << Blob::copies << "c/" << Blob::moves << "m";
    Blob::reset(); f(Blob(64));
    std::cout << "    rvalue: " << Blob::copies << "c/" << Blob::moves << "m\n";
}

void demo_sink() {
    std::cout << "\n=== B. the sink idiom, measured (Blob = counted string) ===\n";
    Recorder r;
    // NOTE the std::forward in EVERY row: the harness binds the caller's value as
    // auto&& b (4.2) — relaying the category is OUR job (4.3). An earlier draft
    // forgot it in the sink row and the rvalue cell quietly measured the lvalue
    // path (1c/1m instead of 0c/2m) — the prediction caught the lie.
    row("const& + copy-in ", [&](auto&& b) { r.set_const_ref(std::forward<decltype(b)>(b)); });
    row("sink T  + move-in ", [&](auto&& b) { r.set_sink(std::forward<decltype(b)>(b)); });
    row("overload pair     ", [&](auto&& b) { r.set_pair(std::forward<decltype(b)>(b)); });
    std::cout << "  conditional store (store == false):\n";
    Blob src(64);
    Blob::reset(); r.maybe_const(src, false);
    std::cout << "    reader const&: " << Blob::copies << "c/" << Blob::moves << "m   (paid nothing)\n";
    Blob::reset(); r.maybe_sink(src, false);
    std::cout << "    sink T       : " << Blob::copies << "c/" << Blob::moves << "m   (paid the copy, stored NOTHING)\n";
    std::cout << "  verdict: sink covers BOTH callers with ONE body (N params -> the pair needs\n";
    std::cout << "  2^N bodies), never pays more than one cheap move over optimal, and recovers\n";
    std::cout << "  the rvalue steal that the const& binding erased. It LOSES when you store\n";
    std::cout << "  conditionally: the boundary copy is paid even when nothing is kept.\n";
}

// ---------------- C. the auto matrix ----------------
// auto(x) = the BY-VALUE PARAMETER transformation as an expression:
// references strip, arrays/functions decay to pointers, classes pass through.
template <class T> decltype(auto) deduce(T&& x) { return x; }            // returns T& : ALIASES
template <class T> decltype(auto) deduce_value(T&& x) { return auto(x); } // returns T  : COPIES

void demo_auto_matrix() {
    std::cout << "\n=== C. the auto matrix (auto(x) needs -std=c++23) ===\n";
    int i = 42;
    int& ir = i;
    auto  v1 = ir;        static_assert(std::is_same_v<decltype(v1), int>);      // copy
    auto& v2 = ir;        static_assert(std::is_same_v<decltype(v2), int&>);     // alias
    auto&& v3 = ir;       static_assert(std::is_same_v<decltype(v3), int&>);     // 4.2 collapsing
    auto&& v4 = 42;       static_assert(std::is_same_v<decltype(v4), int&&>);    // binds + extends (1.6)
    const auto& v6 = 42;  static_assert(std::is_same_v<decltype(v6), const int&>); // extends
    auto  v5 = auto(ir);  static_assert(std::is_same_v<decltype(v5), int>);      // explicit decay copy
    int arr[5] = {};
    auto p = auto(arr);   static_assert(std::is_same_v<decltype(p), int*>);      // array -> pointer
    static_assert(std::is_same_v<decltype(deduce(ir)), int&>);                   // forwarding returns the REF
    static_assert(std::is_same_v<decltype(deduce_value(ir)), int>);              // auto(x) returns the VALUE

    std::cout << "  auto   v = ir;     -> int         (independent copy)\n";
    std::cout << "  auto&  v = ir;     -> int&        (alias)\n";
    std::cout << "  auto&& v = ir;     -> int&        (forwarding ref, 4.2)\n";
    std::cout << "  auto&& v = 42;     -> int&&       (binds prvalue, life extended, 1.6)\n";
    std::cout << "  auto(ir)           -> int         (decay copy: reference stripped)\n";
    std::cout << "  auto(arr)          -> int*        (decay copy: array to pointer)\n";
    std::cout << "  deduce(ir)         -> int&        (decltype(auto) + return x  ALIASES)\n";
    std::cout << "  deduce_value(ir)   -> int         (return auto(x)            COPIES)\n";

    ++v1;  std::cout << "\n  ++v1 (independent int): i stays  " << i << "\n";
    ++v2;  std::cout << "  ++v2 (alias of i)     : i becomes " << i << "\n";
    std::cout << "  (v1 and v2 differ by ONE character; their semantics are the two\n";
    std::cout <<   "   halves of this whole lesson)\n";
}

// ---------------- D. the proxy truth: decay does NOT unwrap handles ----------------
void demo_proxy_truth() {
    std::cout << "\n=== D. auto(x) and proxies: the verified truth ===\n";
    std::vector<bool> bits{true, false, true};
    auto proxy = bits[0];
    auto handle_copy = auto(bits[0]);
    static_assert(std::is_same_v<decltype(proxy), std::vector<bool>::reference>);
    static_assert(std::is_same_v<decltype(handle_copy), std::vector<bool>::reference>);  // NOT bool!
    std::cout << "  auto  proxy = bits[0]; -> vector<bool>::reference (the handle)\n";
    std::cout << "  auto(bits[0])          -> vector<bool>::reference (STILL the handle)\n";
    std::cout << "  decay passes classes through untouched: auto(x) copies the HANDLE,\n";
    std::cout << "  it does not extract the value. If the owner dies, the copy dangles\n";
    std::cout << "  exactly like the original (ASan: heap-use-after-free in _Bit_reference)\n";
    bool b = bits[1];                          // the conversion runs WHILE the owner lives
    std::cout << "  bool b = bits[1];      -> " << b << "   (naming the VALUE type IS the unwrap)\n";
    int count = 0;
    for (bool bit : bits) count += bit;        // same trick in a range-for
    std::cout << "  for (bool bit : bits)  -> " << count << " trues (the proxy never escapes)\n";
    std::cout << "  auto&& b3 = bits[0];  -> binds the proxy safely while bits lives;\n";
    std::cout << "  auto&  b7 = bits[0];  -> COMPILE ERROR: non-const lvalue ref vs prvalue\n";
}

// ---------------- E. evidence already banked (cited, NOT re-run) ----------------
void demo_bank() {
    std::cout << "\n=== E. evidence banked in earlier lessons (cited, not re-run) ===\n";
    std::cout << "  1.10 : 64B lines — row/col traversal 11.34x; false sharing 4.90x\n";
    std::cout << "         (contiguity is value semantics' home turf; sharing crosses threads)\n";
    std::cout << "  3.3  : hand-off through shared handles 6.06 ns vs 0.485 ns direct\n";
    std::cout << "  4.4  : return by value is FREE — the prvalue materializes at the destination\n";
    std::cout << "  4.3  : a const& binding ERASES rvalue-ness — the copy the sink idiom recovers\n";
    std::cout << "  2.8/4.6 : vector growth reads your noexcept signature to choose the era\n";
}

int main() {
    demo_semantics();
    demo_sink();
    demo_auto_matrix();
    demo_proxy_truth();
    demo_bank();
    return 0;
}
