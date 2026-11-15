// lesson_4_5_moved_from.cpp — the moved-from state & the pitfall zoo.
//
// ONE idea: the moved-from state is a CONTRACT — "valid but unspecified" —
// and every pitfall in this lesson is a violation of that contract's edges:
//   - const promises protection, but SILENTLY converts your move to a copy
//   - the husk is valid, but your assumptions about its contents are not
//   - moved-from members in YOUR class are YOUR responsibility (dtor, ==, print)
//
// The zoo: three pitfalls, each TRIGGERED (the bug runs), DETECTED (the
// instrument or sanitizer catches it), then FIXED. Small zoo, deliberately.
//
// Build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_4_5_moved_from.cpp -o mf && ./mf
// MSVC : cl /std:c++20 /EHsc /W4 lesson_4_5_moved_from.cpp

#include <iostream>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------- the instrumented type
struct Payload {
    static inline int copies = 0, moves = 0;
    static void reset() { copies = moves = 0; }
    std::string data;
    explicit Payload(std::string s) : data(std::move(s)) {}
    Payload(const Payload& o) : data(o.data) { ++copies; }
    Payload(Payload&& o) noexcept : data(std::move(o.data)) { ++moves; }
    // declaring the move ctor deleted the copy-assign (2.2's table) — the
    // const-silent-copy demo needs assignment, so both are spelled out:
    Payload& operator=(const Payload& o) { data = o.data; ++copies; return *this; }
    Payload& operator=(Payload&& o) noexcept { data = std::move(o.data); ++moves; return *this; }
};

// ---------------------------------------- pitfall 1: moving from a const object
struct Widget1Real {
    Payload member{"none"};
    void take(const Payload&& p) {             // NOTE the const (the bug)
        member = std::move(p);                 // p is const&&: move BLOCKED,
    }                                          // copy ctor binds -> silent copy
    void fixed(Payload&& p) {                  // the fix: drop the const
        member = std::move(p);                 // p is &&: the move ctor fires
    }
};

void demo_const_move() {
    std::cout << "=== 1. Moving from a const object: the SILENT COPY ===\n";
    Payload p("payload");
    const Payload cp("const-payload");

    Payload::reset();
    Payload a = std::move(p);
    std::cout << "  move(mutable)    : copies=" << Payload::copies
              << " moves=" << Payload::moves << "  (stole: intended)\n";

    Payload::reset();
    Payload b = std::move(cp);                 // legal! but copies
    std::cout << "  move(const)      : copies=" << Payload::copies
              << " moves=" << Payload::moves << "  (COPIED: the silent betrayal)\n";
    std::cout << "  -> std::move(cp) relabeled it const&& — the move ctor wants\n";
    std::cout << "     (Payload&&) and const blocks it; only the copy ctor binds.\n";
    std::cout << "     Compiles. Runs. Duplicates every byte. Says nothing.\n";
    (void)a; (void)b;

    Widget1Real w;
    Payload::reset();
    w.take(Payload("x"));                      // const&& parameter — the bug in API form
    std::cout << "  API form take(const Payload&&): copies=" << Payload::copies
              << " moves=" << Payload::moves << "  (the const&& param was a trap)\n";
    Payload::reset();
    w.fixed(Payload("y"));
    std::cout << "  fixed   take(Payload&&)      : copies=" << Payload::copies
              << " moves=" << Payload::moves << "  (const removed: moves again)\n";
    std::cout << "  -> the lesson generalizes: const anywhere in the chain —\n";
    std::cout << "     const object, const&& param, const member access — kills\n";
    std::cout << "     the steal and buys the copy. Silently. Every time.\n";
}

// ---------------------------------------- pitfall 2: moved-from assumptions
struct Report {
    std::vector<int> samples;
    Report(std::initializer_list<int> il) : samples(il) {}
    // count() after a move: samples is EMPTY (vector's moved-from is empty
    // in practice) — but "empty" is an IMPLEMENTATION detail, not the contract.
    // The contract is only: valid (destructible, assignable) + unspecified.
    std::size_t count() const { return samples.size(); }
};

void demo_assumptions() {
    std::cout << "\n=== 2. The husk is valid — your assumptions are not ===\n";
    Report r{1, 2, 3, 4, 5};
    std::cout << "  before: r.count() = " << r.count() << "\n";
    Report r2 = std::move(r);
    std::cout << "  after move: r.count() = " << r.count()
              << "  (the vector's moved-from IS empty on this implementation —\n";
    std::cout << "   but '== 0' is NOT the contract; 'valid, assignable, unknown\n";
    std::cout << "   contents' is. Relying on 0 works today, breaks on the next\n";
    std::cout << "   type/library/platform that leaves something else behind)\n";
    std::cout << "  the SAFE pattern: moved-from objects get exactly two uses —\n";
    std::cout << "     assign to it, or destroy it. Anything else is a gamble.\n";
    r = Report{9};                             // assignment: always legal, always defined
    std::cout << "  after reassignment: r.count() = " << r.count() << "  (legal: assign)\n";
    (void)r2;
}

// ---------------------------------------- pitfall 3: the moved-from MEMBER
// When YOUR class moves, YOUR members are left in moved-from states, and
// every operation that runs afterwards must cope. Destructors always run.
struct Connection {
    std::string url;
    Payload cache;
    int id;
    Connection(int i, std::string u, Payload c)
        : url(std::move(u)), cache(std::move(c)), id(i) {}
    Connection(Connection&& o) noexcept
        : url(std::move(o.url)), cache(std::move(o.cache)), id(o.id) {
        o.id = -1;                             // explicit husk mark for debugging
    }
};

void demo_members() {
    std::cout << "\n=== 3. Moved-from MEMBERS are your responsibility ===\n";
    Connection c1(7, "db://primary", Payload("session-cache"));
    std::cout << "  c1: id=" << c1.id << " url='" << c1.url
              << "' cache='" << c1.cache.data << "'\n";
    Connection c2 = std::move(c1);
    std::cout << "  after move, c1: id=" << c1.id << " url='" << c1.url
              << "' (empty) cache='" << c1.cache.data << "' (empty)\n";
    std::cout << "  -> ~Connection() still runs for c1: empty strings destroy fine\n";
    std::cout << "     (that's WHY std moved-from types guarantee 'valid'). Your\n";
    std::cout << "     own members: a raw FILE* member would need = nullptr here,\n";
    std::cout << "     or the husk's dtor closes a stolen handle twice.\n";
    std::cout << "  c2: id=" << c2.id << " url='" << c2.url
              << "' cache='" << c2.cache.data << "'\n";
}

int main() {
    demo_const_move();
    demo_assumptions();
    demo_members();
    return 0;
}
