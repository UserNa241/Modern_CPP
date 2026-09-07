// lesson_2_6_scope_guard.cpp — portable, standard C++ only
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_2_6_scope_guard.cpp -o sg && ./sg
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>
#include <exception>

// ---------------- the guards ----------------
template <class F>
class ScopeExit {                       // fires ALWAYS (unless released)
    F f_; bool active_ = true;
public:
    explicit ScopeExit(F f) : f_(std::move(f)) {}
    ~ScopeExit() { if (active_) f_(); }
    void release() noexcept { active_ = false; }
    ScopeExit(const ScopeExit&)            = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
};
template <class F> ScopeExit(F) -> ScopeExit<F>;      // CTAD

template <class F>
class ScopeFail {                       // fires ONLY if unwinding started after birth
    F f_; int born_at_;
public:
    explicit ScopeFail(F f)
        : f_(std::move(f)), born_at_(std::uncaught_exceptions()) {}
    ~ScopeFail() {
        if (std::uncaught_exceptions() > born_at_) f_();
    }
    ScopeFail(const ScopeFail&)            = delete;
    ScopeFail& operator=(const ScopeFail&) = delete;
};
template <class F> ScopeFail(F) -> ScopeFail<F>;

template <class F>
class ScopeSuccess {                    // fires ONLY on the non-throwing path
    F f_; int born_at_;
public:
    explicit ScopeSuccess(F f)
        : f_(std::move(f)), born_at_(std::uncaught_exceptions()) {}
    ~ScopeSuccess() {
        if (std::uncaught_exceptions() == born_at_) f_();
    }
    ScopeSuccess(const ScopeSuccess&)            = delete;
    ScopeSuccess& operator=(const ScopeSuccess&) = delete;
};
template <class F> ScopeSuccess(F) -> ScopeSuccess<F>;

// ---------------- demo 3: commit-or-rollback transaction ----------------
bool add_entry(std::vector<std::string>& ledger, const std::string& e, bool explode_later) {
    ledger.push_back(e);                                   // partial mutation...
    ScopeExit rollback([&] {                               // ...immediately armed
        std::cout << "    [ROLLBACK: removing '" << e << "']\n";
        ledger.pop_back();
    });
    if (explode_later) throw std::runtime_error("validation failed");
    rollback.release();                                    // COMMIT: disarm rollback
    std::cout << "    [COMMIT: '" << e << "' stays]\n";
    return true;
}

int main() {
    std::cout << "=== 1. ScopeExit: fires on every exit ===\n";
    {
        ScopeExit log{[]{ std::cout << "  [leaving scope 1]\n"; }};
        std::cout << "  working...\n";
    }
    try {
        ScopeExit log{[]{ std::cout << "  [leaving via EXCEPTION]\n"; }};
        throw std::runtime_error("boom");
    } catch (const std::exception& e) { std::cout << "  caught " << e.what() << "\n"; }

    std::cout << "\n=== 2. release(): the disarm button ===\n";
    {
        ScopeExit g{[]{ std::cout << "  [would clean up]\n"; }};
        std::cout << "  all fallible work done -> commit\n";
        g.release();
        std::cout << "  (brace passed silently? ";
        std::cout << "yes - nothing printed between here and 'done')\n";
    }
    std::cout << "  done\n";

    std::cout << "\n=== 3. Commit-or-rollback in 5 lines ===\n";
    std::vector<std::string> ledger{"init"};
    try {
        add_entry(ledger, "ok-entry", false);              // commits
        add_entry(ledger, "bad-entry", true);              // throws -> rolls back
    } catch (const std::exception& e) { std::cout << "  caught: " << e.what() << "\n"; }
    std::cout << "  ledger now:";
    for (const auto& s : ledger) std::cout << " '" << s << "'";
    std::cout << "   <- strong guarantee: 'bad-entry' never landed\n";

    std::cout << "\n=== 4. ScopeFail / ScopeSuccess: exception-aware ===\n";
    auto run = [](bool fail) {
        std::cout << "  -- function(fail=" << fail << ") --\n";
        ScopeFail   on_fail{   []{ std::cout << "    [ScopeFail   fired]\n"; }};
        ScopeSuccess on_ok{[]{ std::cout << "    [ScopeSuccess fired]\n"; }};
        if (fail) throw std::runtime_error("inner boom");
    };
    try { run(false); } catch (...) {}
    try { run(true);  } catch (...) {}

    std::cout << "\n=== 5. The counting trick, visible ===\n";
    try {
        std::cout << "  uncaught at start: " << std::uncaught_exceptions() << "\n";
        throw std::runtime_error("outer");
    } catch (...) {
        std::cout << "  uncaught in catch: " << std::uncaught_exceptions() << "\n";
        try { throw std::runtime_error("inner"); }
        catch (...) {
            std::cout << "  nested catch     : " << std::uncaught_exceptions()
                      << "  (still 1: inner was CAUGHT, not unwound)\n";
        }
    }
    return 0;
}
