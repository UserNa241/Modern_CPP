// lesson_4_8_move_only_function.cpp — std::move_only_function (C++23)
//
// ONE idea: a callable wrapper you can MOVE but never COPY.
//
// std::function (C++11) requires its target to be CopyConstructible, because
// copying the wrapper must deep-copy the target. A lambda capturing
// std::unique_ptr, a move-only functor, anything with deleted copy is therefore
// excluded. std::move_only_function (P0288R9, C++23) fixes it: single ownership,
// copy deleted, move is the only transfer, and cv/ref/noexcept are part of the type.
//
// This file uses the STANDARD facility only — no hand-rolled wrapper, and NO
// global operator new/delete replacement (that is what breaks across toolchains).
// Header is <functional>; there is no <move_only_function> header.
//
//   GCC    >= 12      libstdc++
//   Clang  >= 16      libc++
//   MSVC   >= 19.34   VS 2022 17.4, needs /std:c++latest
//
// Build:
//   g++     -std=c++23 -Wall -Wextra -Wpedantic 4.8_move_only_function.cpp -o mof && ./mof
//   clang++ -std=c++23 -Wall -Wextra -Wpedantic 4.8_move_only_function.cpp -o mof && ./mof
//   cl /std:c++latest /permissive- /EHsc /W4 /utf-8 4.8_move_only_function.cpp
//
// Run:
//   mof            segments A..K
//   mof --empty    segment L: the empty-invocation precondition (aborts the process)
//
// HOW SBO IS MEASURED WITHOUT TOUCHING THE ALLOCATOR:
//   the callable's operator() reports its OWN address; we then ask whether that
//   address lies inside the wrapper object. Inside -> stored inline. Outside ->
//   allocated on the heap. Plain pointer comparison: no <cstdlib>, no aligned_alloc,
//   no operator new overrides, nothing that differs between glibc and the MSVC CRT.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <version>
#if !defined(__cpp_lib_move_only_function)
#  error "std::move_only_function unavailable: need GCC>=12, Clang/libc++>=16, or MSVC>=19.34 with /std:c++latest."
#endif

// ---------------------------------------------------------------------------
// Address probe: where does the callable physically live?
// ---------------------------------------------------------------------------
namespace probe {

inline const void*  wrap_begin = nullptr;   // address of the wrapper under test
inline std::size_t  wrap_size  = 0;
inline const void*  last_addr  = nullptr;   // address the callable reported

inline bool inside_wrapper(const void* p) {
    const auto base = reinterpret_cast<std::uintptr_t>(wrap_begin);
    const auto q    = reinterpret_cast<std::uintptr_t>(p);
    return wrap_begin != nullptr && q >= base && q < base + wrap_size;
}

template <class Wrapper>
void watch(const Wrapper& w) {
    wrap_begin = static_cast<const void*>(&w);
    wrap_size  = sizeof(Wrapper);
    last_addr  = nullptr;
}

} // namespace probe

// ---------------------------------------------------------------------------
// Compile-time facts about the standard type
// ---------------------------------------------------------------------------
namespace {
int  returns_int()  { return 1; }   // named, so no lambda types in unevaluated contexts
void returns_void() {}
} // namespace

using mof_void = std::move_only_function<void()>;

static_assert(!std::is_copy_constructible_v<mof_void>,
              "move_only_function is move-only by construction");
static_assert(!std::is_copy_assignable_v<mof_void>,
              "copy assignment is deleted too");
static_assert(std::is_nothrow_move_constructible_v<mof_void>,
              "move must be noexcept so containers can relocate");
static_assert(!std::is_same_v<std::move_only_function<void()>,
                              std::move_only_function<void() noexcept>>,
              "noexcept is part of the type");
static_assert(std::is_constructible_v<mof_void, std::nullptr_t>,
              "constructible from nullptr, meaning empty");
static_assert(std::is_convertible_v<decltype(&returns_int), mof_void>,
              "a value-returning callable converts (R = void discards the value)");
static_assert(!std::is_constructible_v<std::move_only_function<int()>, decltype(&returns_void)>,
              "a void callable cannot become move_only_function<int()>");

// The illegal direction cannot be a static_assert; it is a hard error:
//
//     void may_throw();
//     std::move_only_function<void() noexcept> f = may_throw;
//     error: no matching constructor (the noexcept instantiation requires
//            is_nothrow_invocable_r_v of the target)

// ---------------------------------------------------------------------------
// Test callables
// ---------------------------------------------------------------------------
namespace {

template <int N>
struct Payload {                     // N-byte callable that reports its own address
    char pad[N > 0 ? N : 1];
    void operator()() const { probe::last_addr = static_cast<const void*>(this); }
};

struct Counter {                     // non-const operator(): needs a non-const call
    int n = 0;
    void operator()() { std::cout << "    Counter called, n = " << ++n << '\n'; }
};

// An over-aligned callable: alignof(64) > the default new alignment, so it cannot
// live in the inline buffer. Two details matter for MSVC:
//   * the payload is sized to EXACTLY 64 bytes, so the alignment specifier adds no
//     padding. Otherwise MSVC emits C4324 ("structure was padded due to alignment
//     specifier"), which /W4 /WX promotes to a hard error.
//   * the warning is also disabled for the definition, as a belt-and-braces guard
//     against compilers that report it anyway.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4324)
#endif
struct alignas(64) OverAligned {
    double v[8]{1.0, 2.0};           // 8 * 8 = 64 bytes: no padding needed
    void operator()() const {
        std::cout << "    OverAligned ran (sizeof " << sizeof(OverAligned)
                  << "), address % 64 = "
                  << reinterpret_cast<std::uintptr_t>(this) % 64 << '\n';
    }
};
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

struct Resource {                    // owns a heap object, non-copyable
    std::unique_ptr<int> p;
    explicit Resource(int v) : p(std::make_unique<int>(v)) {}
    void operator()() const { std::cout << "    *p = " << *p << '\n'; }
};

void section(const char* title) { std::cout << "\n=== " << title << " ===\n"; }

// --- per-size measurements, all via address comparison ---------------------
struct Row {
    bool inline_before;
    bool inline_after;
    bool same_address;
};

template <int N>
Row measure() {
    std::move_only_function<void()> a(Payload<N>{});
    probe::watch(a);
    a();
    const void* before = probe::last_addr;
    const bool in_before = probe::inside_wrapper(before);

    std::move_only_function<void()> b(std::move(a));
    probe::watch(b);
    b();
    const void* after = probe::last_addr;

    return Row{in_before, probe::inside_wrapper(after), before == after};
}

template <int N> void scan_step(std::size_t& first_inline_miss) {
    if (first_inline_miss == 0 && !measure<N>().inline_before)
        first_inline_miss = static_cast<std::size_t>(N);
}
template <std::size_t... Ns>
std::size_t scan_impl(std::index_sequence<Ns...>) {
    std::size_t first_inline_miss = 0;
    (scan_step<static_cast<int>(Ns) + 1>(first_inline_miss), ...);
    return first_inline_miss;
}

void print_row(const char* name, const Row& r) {
    std::cout << "    " << name << " | "
              << (r.inline_before ? "inline" : "heap  ") << " | "
              << (r.inline_after  ? "inline" : "heap  ") << " | "
              << (r.same_address ? "pointer stolen, address identical"
                                 : "relocated, address changed") << '\n';
}

} // namespace

int main(int argc, char** argv) {
    const bool demo_empty = (argc > 1) && (std::string_view(argv[1]) == "--empty");

    std::cout << "sizeof(std::move_only_function<void()>)          = "
              << sizeof(std::move_only_function<void()>) << '\n'
              << "sizeof(std::move_only_function<void() noexcept>) = "
              << sizeof(std::move_only_function<void() noexcept>) << '\n'
              << "sizeof(std::function<void()>)                    = "
              << sizeof(std::function<void()>) << '\n'
              << "feature macro __cpp_lib_move_only_function       = "
              << __cpp_lib_move_only_function << '\n';

    // --- A: a move-only callable can be stored at all ----------------------
    section("A. the defect fixed: unique_ptr capture");
    {
        auto task = [p = std::make_unique<int>(42)] {
            std::cout << "    task reads *p = " << *p << '\n';
        };
        std::cout << "    sizeof(lambda) = " << sizeof(task) << '\n';

        std::move_only_function<void()> f(std::move(task));
        std::cout << "    f() -> ";
        f();
    }

    // --- B: move transfers ownership; source is emptied --------------------
    section("B. move transfers ownership");
    {
        std::move_only_function<void()> f = Resource{7};
        std::move_only_function<void()> g = std::move(f);
        std::cout << std::boolalpha
                  << "    after move: bool(f) = " << static_cast<bool>(f)
                  << "   bool(g) = " << static_cast<bool>(g) << '\n';
        std::cout << "    g() -> ";
        g();
    }

    // --- C: inline vs heap, measured by address ----------------------------
    section("C. where does the target live? (measured by address)");
    {
        const std::size_t first_heap = scan_impl(std::make_index_sequence<64>{});
        std::cout << "    smallest payload that does NOT fit inline: " << first_heap << " B\n"
                  << "    => inline capacity for a callable here is " << (first_heap - 1) << " B\n"
                  << "       (wrapper is " << sizeof(std::move_only_function<void()>)
                  << " B total; the rest is control pointer + padding)\n\n";

        std::cout << "    payload | before | after  | move behaviour\n"
                  << "    --------+--------+--------+-------------------------------\n";
        print_row("  8 B   ", measure<8>());
        print_row(" 16 B   ", measure<16>());
        print_row(" 24 B   ", measure<24>());
        print_row(" 32 B   ", measure<32>());
        print_row(" 64 B   ", measure<64>());
        std::cout << "    The inline threshold is QoI, not the standard: libstdc++, libc++\n"
                  << "    and MSVC all differ. Your program must not depend on it.\n";
    }

    // --- D: noexcept is part of the type -----------------------------------
    section("D. noexcept is part of the type");
    {
        std::move_only_function<void() noexcept> nf = []() noexcept {
            std::cout << "    noexcept callable via noexcept-typed wrapper\n";
        };
        nf();
        std::cout << std::boolalpha
                  << "    is_same_v<void(), void() noexcept> = "
                  << std::is_same_v<std::move_only_function<void()>,
                                    std::move_only_function<void() noexcept>> << '\n'
                  << "    both are "
                  << sizeof(std::move_only_function<void()>) << " bytes: noexcept costs nothing at runtime\n";
    }

    // --- E: non-const target (mutable call state) --------------------------
    section("E. non-const target: mutable call state");
    {
        std::move_only_function<void()> c(Counter{});
        c();
        c();
        c();
        std::cout << "    (works because operator() is NON-const, unlike std::function)\n";
    }

    // --- F: R = void discards a returned value -----------------------------
    section("F. R = void discards a returned value");
    {
        std::move_only_function<void()> discard = [] {
            std::cout << "    lambda returns int 7, wrapper returns void\n";
            return 7;
        };
        discard();
        std::move_only_function<int()> keep = [] { return 6 * 7; };
        std::cout << "    R = int: value preserved -> " << keep() << '\n';
    }

    // --- G: registry of move-only callbacks, no reserve() ------------------
    section("G. vector of move-only callbacks grows without reserve()");
    {
        std::vector<std::move_only_function<int()>> v;
        for (int i = 0; i < 8; ++i)
            v.emplace_back([i] { return i * i; });
        std::cout << "    growth: ";
        for (auto& cb : v) std::cout << cb() << ' ';
        std::cout << "\n    size = " << v.size()
                  << "  (reallocation moved, never copied)\n";
    }

    // --- H: over-aligned target --------------------------------------------
    section("H. over-aligned target (alignas(64))");
    {
        std::move_only_function<void()> o(OverAligned{});
        o();
        std::move_only_function<void()> o2 = std::move(o);
        o2();
        std::cout << "    address identical both times: the move was a pointer steal\n";
    }

    // --- I: self-move-assignment -------------------------------------------
    section("I. self-move-assignment (standard says: unspecified)");
    {
        std::move_only_function<void()> f = [] { std::cout << "    still callable\n"; };
        auto& alias = f;
        f = std::move(alias);
        std::cout << std::boolalpha
                  << "    after f = std::move(f): bool(f) = " << static_cast<bool>(f) << '\n';
        if (f) f(); else std::cout << "    target was released\n";
    }

    // --- J: swap and comparison to nullptr ---------------------------------
    section("J. swap and comparison to nullptr");
    {
        std::move_only_function<int()> a = [] { return 1; };
        std::move_only_function<int()> b = [] { return 2; };
        std::cout << "    before swap: a=" << a() << " b=" << b() << '\n';
        std::swap(a, b);
        std::cout << "    after  swap: a=" << a() << " b=" << b() << '\n';
        std::move_only_function<int()> empty;
        std::cout << std::boolalpha
                  << "    empty == nullptr -> " << (empty == nullptr)
                  << "   a == nullptr -> " << (a == nullptr) << '\n';
    }

    // --- K: in_place_type construction -------------------------------------
    section("K. in_place_type: construct the target directly, no temporary");
    {
        std::move_only_function<void()> f(std::in_place_type<Resource>, 99);
        std::cout << "    f() -> ";
        f();
        std::move_only_function<void()> ctor(std::in_place_type<Counter>);
        ctor();
        ctor();
        std::cout << "    (constructs the target in place: no Resource temporary exists)\n";
    }

    // --- L: the empty-invocation precondition ------------------------------
    if (demo_empty) {
        section("L. empty invocation (precondition violation: undefined behaviour)");
        std::move_only_function<void()> empty;
        std::cout << std::boolalpha
                  << "    bool(empty) = " << static_cast<bool>(empty)
                  << "  -- calling now\n" << std::flush;
        empty();                     // UB. std does NOT throw bad_function_call here.
        std::cout << "    never reached\n";
    } else {
        std::cout << "\n(segment L skipped: run with --empty to see the precondition violation)\n";
    }
}
