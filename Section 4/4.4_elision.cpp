// lesson_4_4_elision.cpp — RVO, NRVO, and guaranteed copy elision (C++17).
//
// ONE idea: a prvalue is not an object — it is a RECIPE for constructing one,
// and the object is constructed directly wherever the recipe is consumed.
// "Elision" is a misleading name for C++17 prvalues: nothing is optimized
// away; there was never anything to remove. NRVO is different: a real object
// in a real frame really is optimized away — and the compiler may decline.
//
//   1. return Widget();  — prvalue return: zero moves in EVERY mode (guaranteed)
//   2. return w;         — NRVO: zero when it fires, ONE move when it can't
//   3. when NRVO can't fire (two returns) — and the -fno-elide spread
//   4. sink(prvalue)     — parameter materialization: direct construct, no move
//   5. the C++14 world: same code, prvalue return COSTS a move (std=c++14!)
//   6. the chain factory — elision + forwarding: the full pipeline, counted
//
// Build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_4_4_elision.cpp -o el && ./el
// Compare: g++ -std=c++14 ... ; and g++ -std=c++20 -fno-elide-constructors ...
// MSVC : cl /std:c++20 /EHsc /W4 lesson_4_4_elision.cpp   (elision always on
//        in conforming mode; /Zc:copy-elision context in the walkthrough)

#include <iostream>
#include <string>
#include <utility>

struct W {
    static inline int born = 0, copies = 0, moves = 0;
    static void reset() { born = copies = moves = 0; }
    std::string tag;
    explicit W(std::string t) : tag(std::move(t)) { ++born; }
    W(const W& o) : tag(o.tag) { ++copies; }
    W(W&& o) noexcept : tag(std::move(o.tag)) { ++moves; }
};

// ---------------------------------------- 1. prvalue return (URVO)
W make_prvalue() {
    return W("direct");          // C++17: the W is constructed IN THE CALLER.
}                                // No local ever exists. Guaranteed, not optimized.

// ---------------------------------------- 2. NRVO
W make_named() {
    W local("named");            // a real object in a real frame...
    return local;                // ...which the compiler may MERGE with the
}                                // caller's destination. If it can't: ONE move.

// ---------------------------------------- 3. two returns: NRVO's classic decline
W maybe(bool b) {
    W a("first"), c("second");
    if (b) return a;             // two candidate sources -> no single destination
    return c;                    // to merge with -> GCC declines NRVO here
}

// ---------------------------------------- 4. parameter materialization
void sink(W byval) {             // the prvalue argument is constructed HERE,
    std::cout << "      sink got '" << byval.tag << "'\n";   // in the parameter
}

// ---------------------------------------- 5. the C++14 world
// (same functions — the difference is the -std flag, see the build matrix)

// ---------------------------------------- 6. the chain factory
template <class T, class... A>
T make(A&&... a) {               // the 3.5/4.3 line
    return T(std::forward<A>(a)...);
}

int main() {
    std::cout << "=== 1. return W(...) — prvalue return ===\n";
    W::reset();
    W w1 = make_prvalue();
    std::cout << "  born=" << W::born << " copies=" << W::copies << " moves=" << W::moves
              << "  (C++20 default build)\n";
    std::cout << "  -> the W was never anywhere but w1. Not 'optimized away' —\n";
    std::cout << "     there was never another object to remove.\n";

    std::cout << "\n=== 2. return local; — NRVO ===\n";
    W::reset();
    W w2 = make_named();
    std::cout << "  born=" << W::born << " copies=" << W::copies << " moves=" << W::moves
              << "  (local merged into w2 — GCC accepted)\n";

    std::cout << "\n=== 3. two returns — NRVO's classic decline ===\n";
    W::reset();
    W w3 = maybe(true);
    std::cout << "  born=" << W::born << " copies=" << W::copies << " moves=" << W::moves
              << "  (2 born: a and c; 1 move: the chosen one crossed)\n";

    std::cout << "\n=== 4. sink(prvalue) — parameter materialization ===\n";
    W::reset();
    sink(W("arg"));
    std::cout << "  born=" << W::born << " copies=" << W::copies << " moves=" << W::moves
              << "  (constructed directly in byval — no move crossed the boundary)\n";

    std::cout << "\n=== 5. the flag matrix (same code, three compiles) ===\n";
    std::cout << "  -std=c++20            : prvalue-ret 0mv | NRVO 0mv | maybe 1mv\n";
    std::cout << "  -std=c++14            : prvalue-ret ??? | NRVO 0mv | maybe 1mv  <- run it\n";
    std::cout << "  -std=c++20 -fno-elide : prvalue-ret 0mv | NRVO 1mv | maybe 1mv  <- run it\n";
    std::cout << "  (prvalue return stays 0 under -fno-elide-constructors: it is\n";
    std::cout << "   GUARANTEED elision — the flag cannot turn it off; only NRVO,\n";
    std::cout << "   an optimization, obeys the flag. C++14 has no guarantee.)\n";

    std::cout << "\n=== 6. the chain: make<W>(\"x\") + forwarding, end to end ===\n";
    W::reset();
    auto w6 = make<W>(std::string("chain"));
    std::cout << "  born=" << W::born << " copies=" << W::copies << " moves=" << W::moves
              << "  (the recipe forwarded, materialized once, at the end)\n";
    std::cout << "  -> 4.3's 'two boundaries' dissolve to zero here: no parameter\n";
    std::cout << "     object, no returned local — prvalue all the way down.\n";

    std::cout << "\n=== totals ===\n";
    std::cout << "  this run: born=" << W::born << " copies=" << W::copies
              << " moves=" << W::moves << " (across sections 6 only; cumulative)\n";
    return 0;
}
