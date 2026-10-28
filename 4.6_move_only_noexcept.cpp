// lesson_4_6_standalone.cpp — move-only types, noexcept as contract,
// [[nodiscard]], and the clang-tidy layer. (Standalone Version)
//
// Build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_4_6_standalone.cpp -o mo && ./mo

#include <iostream>
#include <future>
#include <thread>
#include <sstream>
#include <type_traits>
#include <utility>
#include <memory>

// =========================================================================
// PART 0 — THE FIXTURES (Inlined from Lessons 2.8 and 3.4)
// =========================================================================

// --- From 2.8: The Move Checkers ---
struct MoveOK {
    MoveOK() = default;
    MoveOK(MoveOK&&) noexcept {}
    MoveOK& operator=(MoveOK&&) noexcept { return *this; }
};

struct MoveThrows {
    MoveThrows() = default;
    MoveThrows(const MoveThrows&) {}
    MoveThrows& operator=(const MoveThrows&) { return *this; }
    MoveThrows(MoveThrows&&) /* NOT noexcept */ {}
    MoveThrows& operator=(MoveThrows&&) { return *this; }
};

// --- From 2.8: The Rule of Five Fixture ---
struct Ledger {
    static inline int allocs = 0, frees = 0, copies = 0, moves = 0;
};

class Buffer {
    int* data = nullptr;
    size_t size = 0;
public:
    explicit Buffer(size_t s) : data(new int[s]), size(s) { ++Ledger::allocs; }
    ~Buffer() { if (data) { delete[] data; ++Ledger::frees; } }

    Buffer(const Buffer& o) : data(new int[o.size]), size(o.size) {
        ++Ledger::allocs;
        ++Ledger::copies;
    }
    Buffer& operator=(const Buffer& o) {
        if (this != &o) {
            auto tmp = new int[o.size];
            ++Ledger::allocs;
            if (data) { delete[] data; ++Ledger::frees; }
            data = tmp;
            size = o.size;
            ++Ledger::copies;
        }
        return *this;
    }
    Buffer(Buffer&& o) noexcept : data(o.data), size(o.size) {
        o.data = nullptr; o.size = 0;
        ++Ledger::moves;
    }
    Buffer& operator=(Buffer&& o) noexcept {
        if (this != &o) {
            if (data) { delete[] data; ++Ledger::frees; }
            data = o.data; size = o.size;
            o.data = nullptr; o.size = 0;
            ++Ledger::moves;
        }
        return *this;
    }
};

// --- From 3.4: Custom unique_ptr and Deleter ---
struct TraceDel {
    std::ostringstream* log;
    int val;
    void operator()(int* p) const {
        if (log) *log << "  [del#" << val << " deletes value " << *p << "]\n";
        delete p;
    }
};

namespace my {
    template <class T, class D = std::default_delete<T>>
    class unique_ptr {
        T* ptr = nullptr;
        D deleter;
    public:
        unique_ptr() noexcept = default;
        unique_ptr(T* p) noexcept : ptr(p) {}
        unique_ptr(T* p, D d) noexcept : ptr(p), deleter(std::move(d)) {}
        ~unique_ptr() { reset(); }

        // Copies deleted (The Contract)
        unique_ptr(const unique_ptr&) = delete;
        unique_ptr& operator=(const unique_ptr&) = delete;

        // Moves noexcept (The Contract)
        unique_ptr(unique_ptr&& o) noexcept : ptr(o.ptr), deleter(std::move(o.deleter)) {
            o.ptr = nullptr;
        }
        unique_ptr& operator=(unique_ptr&& o) noexcept {
            if (this != &o) {
                reset();
                ptr = o.ptr;
                deleter = std::move(o.deleter);
                o.ptr = nullptr;
            }
            return *this;
        }

        T* release() noexcept { T* old = ptr; ptr = nullptr; return old; }
        void reset(T* p = nullptr) noexcept {
            T* old = ptr; ptr = p;
            if (old) deleter(old);
        }
    };

    // Array specialization (just enough for the trait audit)
    template <class T, class D>
    class unique_ptr<T[], D> {
    public:
        unique_ptr() noexcept = default;
        unique_ptr(const unique_ptr&) = delete;
        unique_ptr& operator=(const unique_ptr&) = delete;
        unique_ptr(unique_ptr&&) noexcept = default;
        unique_ptr& operator=(unique_ptr&&) noexcept = default;
    };

    template <class T, class... Args>
    unique_ptr<T> make_unique(Args&&... args) {
        return unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}


// =========================================================================
// PART 1 — Constraint A as a trait audit (the compile-time half)
// =========================================================================

template <class T>
bool audit_row(const char* name, bool expect_pass) {
    const bool noncopy = !std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T>;
    const bool moves   =  std::is_move_constructible_v<T>  &&  std::is_move_assignable_v<T>;
    const bool nothrow =  std::is_nothrow_move_constructible_v<T>
                       &&  std::is_nothrow_move_assignable_v<T>;
    const bool pass = noncopy && moves && nothrow;
    std::cout << "  " << name << (pass ? "PASS" : "fail")
              << "  [non-copyable=" << noncopy << "  movable=" << moves
              << "  nothrow-move=" << nothrow << "]"
              << (pass == expect_pass ? "" : "   *** UNEXPECTED ***") << "\n";
    return pass == expect_pass;
}

// Full contract at compile time — stops building if a signature breaks:
static_assert(!std::is_copy_constructible_v<my::unique_ptr<int>>);
static_assert(!std::is_copy_assignable_v<my::unique_ptr<int>>);
static_assert( std::is_nothrow_move_constructible_v<my::unique_ptr<int>>);
static_assert( std::is_nothrow_move_assignable_v<my::unique_ptr<int>>);

// The suppression table as a DESIGN TOOL: declaring a move deletes copies implicitly.
struct MoveOnlyByDeclaration {
    MoveOnlyByDeclaration(MoveOnlyByDeclaration&&) noexcept = default;
};
static_assert(!std::is_copy_constructible_v<MoveOnlyByDeclaration>);
static_assert(!std::is_copy_assignable_v<MoveOnlyByDeclaration>);

// move_if_noexcept: The library's compile-time branch!
static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<MoveOK&>())), MoveOK&&>);
// nothrow move -> rvalue: STEAL

struct ThrowNoCopy {                            // the third branch
    ThrowNoCopy(ThrowNoCopy&&);                 // throwing move, and no copy at all
    ThrowNoCopy(const ThrowNoCopy&) = delete;
};

static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<MoveThrows&>())),const MoveThrows&>);
// throwing move + copyable -> const&: COPY
static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<ThrowNoCopy&>())), ThrowNoCopy&&>);
// throwing move + no copy -> rvalue anyway: gamble


// =========================================================================
// PART 2 — [[nodiscard]]: the handle you must not drop
// =========================================================================

[[nodiscard]] my::unique_ptr<int, TraceDel> open_ticket(int v, std::ostringstream* log) {
    return my::unique_ptr<int, TraceDel>(my::make_unique<int>(v).release(),
                                         TraceDel{log, v});
}


// =========================================================================
// THE RUNTIME SUITE
// =========================================================================

int main() {
    int failures = 0;
    std::cout << "=== A. Constraint A trait audit — the canon and the artifacts ===\n";
    failures += !audit_row<my::unique_ptr<int>>         ("my::unique_ptr<int>        ", true);
    failures += !audit_row<my::unique_ptr<int[]>>       ("my::unique_ptr<int[]>      ", true);
    failures += !audit_row<my::unique_ptr<int,TraceDel>>("my::unique_ptr<int,TraceDel>", true);
    failures += !audit_row<std::unique_ptr<int>>        ("std::unique_ptr<int>       ", true);
    failures += !audit_row<std::thread>                 ("std::thread                ", true);
    failures += !audit_row<std::promise<int>>           ("std::promise<int>          ", true);
    failures += !audit_row<std::future<int>>            ("std::future<int>           ", true);
    failures += !audit_row<Buffer>                      ("Buffer  (2.8 Rule of Five) ", false);
    failures += !audit_row<std::string>                 ("std::string  (value type)  ", false);
    std::cout << "  (the last two rows are EXPECTED fails — Buffer is the copyable\n";
    std::cout << "   Rule-of-Five flavor, string is a value type. A suite that cannot\n";
    std::cout << "   say no is a suite whose yes means nothing.)\n";

    std::cout << "\n=== B. runtime: husk validity + self-move — the fixtures narrate ===\n";
    {
        std::ostringstream dellog;
        {
            auto t = open_ticket(41, &dellog);   // factory: handle acquired
            auto u = std::move(t);               // move ctor: t is a husk now
            auto& alias = u;                     // self-move via alias
            u = std::move(alias);                // reset(release()) order-guarded
            t = open_ticket(99, &dellog);        // HUSK REASSIGNED — the "valid" half

            // open_ticket(7, &dellog);          // UNCOMMENT THIS to see [[nodiscard]] fail the build!
        }
        const std::string ledger = dellog.str();
        std::cout << ledger;
        const std::string expected = "  [del#41 deletes value 41]\n  [del#99 deletes value 99]\n";
        const bool ok = ledger == expected;
        std::cout << "  " << (ok ? "CHECK" : "*** FAIL")
                  << ": ledger byte-exact — each handle deleted exactly once,\n"
                  << "     self-move deleted NOTHING, the husk outlived its move and\n"
                  << "     accepted a fresh ticket before dying empty.\n";
        failures += !ok;
    }
    {
        std::cout << "\n--- Buffer (2.8's Rule-of-Five fixture): the copyable flavor ---\n";
        Ledger::allocs = Ledger::frees = Ledger::copies = Ledger::moves = 0;
        {
            Buffer b(8);              // alloc #1
            Buffer c = b;             // 1 deep copy
            Buffer d = std::move(b);  // 1 steal; b is a husk
            auto& alias = d;
            d = std::move(alias);     // self-move: guarded
            b = Buffer(4);            // husk REASSIGNED via move-assign (alloc #2)
        }                             // 3 buffers destroyed: frees == allocs + the copy's allocation
        std::cout << "  ledger: allocs=" << Ledger::allocs << " frees=" << Ledger::frees
                  << " copies=" << Ledger::copies << " moves=" << Ledger::moves << "\n";
        const bool ok = Ledger::allocs + Ledger::copies == Ledger::frees
                     && Ledger::allocs == 2 && Ledger::copies == 1 && Ledger::moves == 2;
        std::cout << "  " << (ok ? "CHECK" : "*** FAIL")
                  << ": allocs(2)+copies(1)==frees(3), 2 steals, self-move guarded\n";
        failures += !ok;
    }

    std::cout << "\nVERDICT: " << (failures == 0 ? "Paired Constraint A holds "
                              "against the artifacts"
                              : "FAILURES — see above") << "\n";
    return failures ? 1 : 0;
}