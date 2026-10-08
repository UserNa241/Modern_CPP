// lesson_4_2_move_collapsing.cpp — std::move, forwarding references, collapsing.
//
// ONE idea: T&& is one spelling with two languages. FIXED (Widget&&) it means
// "rvalue only". DEDUCED (T&& with T being deduced, auto&&) it means "anything
// — and T itself remembers which". Reference collapsing is the grammar that
// makes the second language work; std::move is just a cast into the first.
//
//   1. std::move is a cast — my::move, and proof it moves NOTHING by itself
//   2. the deduction matrix: what T becomes for 4 argument kinds
//   3. the four collapsing rules (compile-time, via aliases)
//   4. auto&& — the deduced form in auto clothing
//   5. the absorbed rvalue: inside f(T&& x), x is an lvalue — the 4.3 teaser
//
// Build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_4_2_move_collapsing.cpp -o mc && ./mc
// MSVC : cl /std:c++20 /EHsc /W4 lesson_4_2_move_collapsing.cpp

#include <iostream>
#include <string_view>
#include <type_traits>
#include <utility>

struct Tag {};   // needed by type_name (section 5) — defined early

// ---------------------------------------- a name for every case (compile-time)
template <class T>
constexpr std::string_view type_name() {
    if constexpr (std::is_same_v<T, int>)            return "int";
    else if constexpr (std::is_same_v<T, int&>)      return "int&";
    else if constexpr (std::is_same_v<T, const int&>)  return "const int&";
    else if constexpr (std::is_same_v<T, int&&>)     return "int&&";
    else if constexpr (std::is_same_v<T, const int&&>) return "const int&&";
    else if constexpr (std::is_same_v<T, const int>) return "const int";
    else if constexpr (std::is_same_v<T, Tag>)       return "Tag";
    else if constexpr (std::is_same_v<T, Tag&>)      return "Tag&";
    else return "something-else";
}

// ---------------------------------------- 1. std::move IS the cast
// This is the entire implementation (remove_reference unwraps T=int& -> int):
template <class T>
constexpr std::remove_reference_t<T>&& my_move(T&& t) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(t);
}

static inline long m_ct = 0, c_ct = 0;
struct Tr {
    Tr() = default;
    Tr(const Tr&) { ++c_ct; }
    Tr(Tr&&) noexcept { ++m_ct; }
};

void demo_move_is_a_cast() {
    std::cout << "=== 1. std::move is a cast — it moves NOTHING by itself ===\n";
    Tr t;
    my_move(t);                       // a relabel, evaluated, discarded: NOTHING happens
    std::cout << "  after my_move(t) alone   : moves=" << m_ct << " copies=" << c_ct << "\n";
    (void)t;
    Tr u = my_move(t);                // HERE the relabel lets overload resolution pick &&
    std::cout << "  after Tr u = my_move(t)  : moves=" << m_ct << " copies=" << c_ct << "\n";
    (void)u;
    std::cout << "  (and static_cast<Tr&&>(t) compiles to the identical call —\n";
    std::cout << "   see the walkthrough's two-line disassembly)\n";
}

// ---------------------------------------- 2. the deduction matrix
// Ded captures BOTH the deduced T and the collapsed parameter type T&&.
template <class T>
struct Ded {
    using deduced = T;
    using param   = T&&;             // collapsed by the rules (section 3)
};
template <class T>
Ded<T> ded(T&&);                    // never defined: used only in decltype

// namespace-scope compile-time matrix — the table IS these four asserts:
static_assert(std::is_same_v<typename decltype(ded(std::declval<int&>()))::deduced, int&> &&
              std::is_same_v<typename decltype(ded(std::declval<int&>()))::param, int&>);
static_assert(std::is_same_v<typename decltype(ded(std::declval<int&&>()))::deduced, int> &&
              std::is_same_v<typename decltype(ded(std::declval<int&&>()))::param, int&&>);
static_assert(std::is_same_v<typename decltype(ded(std::declval<const int&>()))::deduced, const int&> &&
              std::is_same_v<typename decltype(ded(std::declval<const int&>()))::param, const int&>);
static_assert(std::is_same_v<typename decltype(ded(std::declval<const int&&>()))::deduced, const int> &&
              std::is_same_v<typename decltype(ded(std::declval<const int&&>()))::param, const int&&>);

template <class T>
void show_row(std::string_view what, T&&) {
    std::cout << "  " << what << " -> T = " << type_name<typename Ded<T>::deduced>()   // hmm: T here IS the deduced one
              << ", param = " << type_name<typename Ded<T>::param>() << "\n";
}
// note: inside show_row, T is already the deduced type — the same deduction as ded()

void demo_matrix() {
    std::cout << "\n=== 2. The deduction matrix: template <class T> void f(T&& x) ===\n";
    int lv = 1;
    const int clv = 2;
    show_row("f(lv)            lvalue        ", lv);
    show_row("f(clv)           const lvalue  ", clv);
    show_row("f(7)             rvalue        ", 7);
    show_row("f(my_move(clv))  const rvalue  ", my_move(clv));
    std::cout << "  rule: lvalue  -> T is an LVALUE REF (T=int&),  param collapses to int&\n";
    std::cout << "        rvalue  -> T is the VALUE TYPE  (T=int),  param stays    int&&\n";
    std::cout << "  -> the value category is stored IN T, not in the param's spelling\n";
}

// ---------------------------------------- 3. the four collapsing rules
void demo_collapsing() {
    std::cout << "\n=== 3. Reference collapsing: the four rules ===\n";
    using L = int&;
    using R = int&&;
    static_assert(std::is_same_v<L&, int&>);    // &  &  -> &
    static_assert(std::is_same_v<L&&, int&>);   // &  && -> &
    static_assert(std::is_same_v<R&, int&>);    // && &  -> &
    static_assert(std::is_same_v<R&&, int&&>);  // && && -> &&
    std::cout << "  int&  &  -> int&      int&  && -> int&\n";
    std::cout << "  int&& &  -> int&      int&& && -> int&&\n";
    std::cout << "  (one lvalue reference anywhere -> lvalue reference.\n";
    std::cout << "   collapsing arises ONLY through deduction, aliases, decltype,\n";
    std::cout << "   and auto — you can never write 'int& &&' yourself)\n";
}

// ---------------------------------------- 4. auto&& — the deduced form
void demo_auto() {
    std::cout << "\n=== 4. auto&& — a forwarding reference in auto clothing ===\n";
    int lv = 1;
    const int clv = 2;
    auto&& a1 = lv;
    auto&& a2 = 42;
    auto&& a3 = clv;
    static_assert(std::is_same_v<decltype(a1), int&>);
    static_assert(std::is_same_v<decltype(a2), int&&>);
    static_assert(std::is_same_v<decltype(a3), const int&>);
    std::cout << "  auto&& a1 = lv;   -> " << type_name<decltype(a1)>() << "\n";
    std::cout << "  auto&& a2 = 42;   -> " << type_name<decltype(a2)>() << "\n";
    std::cout << "  auto&& a3 = clv;  -> " << type_name<decltype(a3)>() << "\n";
    std::cout << "  (decltype note: decltype(a1) is the DECLARED type of the name —\n";
    std::cout << "   decltype((a1)) with double parens is int& for all three: a NAME\n";
    std::cout << "   is an lvalue — 1.5's axis distinction, load-bearing in 4.3)\n";
}

// ---------------------------------------- 5. the absorbed rvalue (4.3 teaser)
const char* pick(Tag&)       { return "Tag&"; }
const char* pick(const Tag&) { return "const Tag&"; }
const char* pick(Tag&&)      { return "Tag&&"; }

template <class T>
void relay(T&& x) {
    std::cout << "  relay: T = " << type_name<T>()
              << ", and x (a NAME) is an lvalue -> pick(" << pick(x) << ") fires\n";
}

void demo_absorbed() {
    std::cout << "\n=== 5. The absorbed rvalue: inside f(T&& x), x is an lvalue ===\n";
    std::cout << "  relay(Tag{}):  T should say rvalue... but the CALL inside:\n";
    relay(Tag{});
    Tag g;
    relay(g);
    std::cout << "  -> rvalue-ness arrived, was absorbed INTO T, and x is just a name.\n";
    std::cout << "     pick(x) can never see it. Restoring it is EXACTLY what\n";
    std::cout << "     std::forward<T>(x) does — Lesson 4.3.\n";
}

// ---------------------------------------- fixed T&& (for contrast, compile-time)
struct W {};
void takes_fixed(W&&);        // FIXED: no deduction -> rvalues only
// W w; takes_fixed(w);       // compile error: cannot bind lvalue to W&&
// (uncomment to see it — this is the FIXED language of T&&)

int main() {
    demo_move_is_a_cast();
    demo_matrix();
    demo_collapsing();
    demo_auto();
    demo_absorbed();
    std::cout << "\n  (takes_fixed(W&&) with an lvalue is a compile error — commented in source)\n";
    return 0;
}
