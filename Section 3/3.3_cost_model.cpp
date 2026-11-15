// lesson_3_3_cost_model.cpp — The cost model: every claim measured, none guessed.
//
// What this program measures:
//   1. sizes        : the handle you hold (1 pointer vs 2 pointers)
//   2. allocations  : shared_ptr<T>(new T) = 2 vs make_shared = 1 — counted live
//   2b. the hostage : a weak_ptr keeps the WHOLE make_shared chunk alive (4 KiB demo)
//   3. reads        : dereference cost through shared_ptr vs raw — should be identical
//   4. hand-off     : shared copy/destroy (atomic) vs unique move vs raw copy, 1 thread
//   5. contention   : 2 threads on ONE control block vs private blocks vs no atomics
//
// Build (GCC/Clang): g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -pthread lesson_3_3_cost_model.cpp -o cm
// Build (MSVC):      cl /std:c++20 /EHsc /O2 lesson_3_3_cost_model.cpp
//
// -O2 is not optional here: we benchmark the code that ships, not the code that debugs.
// Numbers are machine-specific. RATIOS are the message.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <thread>
#include <utility>
#include <vector>

// ---------------------------------------------------------- 0. the counting allocator
// Every heap allocation and free in this process is audited. Deltas around a
// region of code tell us exactly what that code allocated and when it came back.
//
// GCC 13+ -Wmismatched-new-delete cannot see through a replaced operator new
// (ours returns malloc() memory, so free() here is CORRECT). Known false
// positive; silence it on GCC only. Clang/MSVC are unaffected.
std::size_t g_allocs = 0, g_frees = 0, g_bytes_in = 0, g_bytes_out = 0;

void* operator new(std::size_t n) {
    ++g_allocs;
    g_bytes_in += n;
    if (void* p = std::malloc(n))
        return p;
    throw std::bad_alloc{};
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
void operator delete(void* p) noexcept {
    ++g_frees;
    g_bytes_out += 0; // size unknown here; byte accounting uses the sized overload + new[]
    std::free(p);
}
void operator delete(void* p, std::size_t n) noexcept {
    ++g_frees;
    g_bytes_out += n;
    std::free(p);
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t n) noexcept { ::operator delete(p, n); }

struct Widget {
    long value = 7;
    explicit Widget(long v = 7) : value(v) {}
};

struct Big { // 4 KiB payload: makes the weak_ptr hostage visible in the byte ledger
    char tag;
    char payload[4096];
    explicit Big(char t) : tag(t) { std::cout << "    +Big '" << t << "'\n"; }
    ~Big() { std::cout << "    -Big '" << tag << "'\n"; }
};

struct Chunk { char pad[128]; }; // allocation wall: keeps two hot control blocks off one cache line

using Clock = std::chrono::steady_clock;

static double ns_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::nano>(Clock::now() - t0).count();
}

static void pns(const char* label, double ns) {
    std::cout << "  " << std::left << std::setw(32) << label << " = " << std::fixed
              << std::setprecision(3) << ns << " ns/op\n";
}

int main() {

    std::cout << "=== 1. Sizes: the handle you hold in your hand ===\n";
    std::cout << "  sizeof(Widget*)             = " << sizeof(Widget*) << "   (raw)\n";
    std::cout << "  sizeof(unique_ptr<Widget>)  = " << sizeof(std::unique_ptr<Widget>)
              << "   (1 pointer + EBO deleter, 3.1)\n";
    std::cout << "  sizeof(shared_ptr<Widget>)  = " << sizeof(std::shared_ptr<Widget>)
              << "   (2 pointers: T* + control-block*)\n";
    std::cout << "  sizeof(weak_ptr<Widget>)    = " << sizeof(std::weak_ptr<Widget>)
              << "   (same 2 pointers)\n";

    std::cout << "\n=== 2. Allocations: counted by the global new/delete audit ===\n";
    constexpr std::size_t K = 1000;
    {
        std::size_t a0 = g_allocs;
        {
            std::vector<std::shared_ptr<Widget>> v;
            v.reserve(K);
            for (std::size_t i = 0; i < K; ++i)
                v.emplace_back(new Widget);
        }
        std::cout << "  shared_ptr<Widget>(new Widget) x" << K << " : " << g_allocs - a0
                  << " allocs   (expected " << 2 * K + 1 << ": K objects + K blocks + vector)\n";
    }
    {
        std::size_t a0 = g_allocs;
        {
            std::vector<std::shared_ptr<Widget>> v;
            v.reserve(K);
            for (std::size_t i = 0; i < K; ++i)
                v.emplace_back(std::make_shared<Widget>());
        }
        std::cout << "  make_shared<Widget>()          x" << K << " : " << g_allocs - a0
                  << " allocs   (expected " << K + 1 << ": K chunks [block|T] + vector)\n";
    }

    std::cout << "\n=== 2b. The weak_ptr hostage: WHEN the memory comes back ===\n";
    {
        std::size_t base = g_bytes_in - g_bytes_out;
        std::weak_ptr<Big> w;
        {
            auto sp = std::make_shared<Big>('M'); // ONE chunk: [block | 4 KiB Big]
            w = sp;
        }                                        // strong -> 0: ~Big runs NOW
        std::cout << "  make_shared: after ~Big        still held = "
                  << (g_bytes_in - g_bytes_out) - base << " bytes\n";
        w.reset();                               // weak -> 0: chunk finally freed
        std::cout << "  make_shared: after weak.reset() still held = "
                  << (g_bytes_in - g_bytes_out) - base << " bytes\n";
    }
    {
        std::size_t base = g_bytes_in - g_bytes_out;
        std::weak_ptr<Big> w;
        {
            std::shared_ptr<Big> sp(new Big('T')); // TWO allocs: Big | block
            w = sp;
        }                                          // strong -> 0: ~Big AND its storage freed
        std::cout << "  two-step:   after ~Big        still held = "
                  << (g_bytes_in - g_bytes_out) - base << " bytes\n";
        w.reset();
        std::cout << "  two-step:   after weak.reset() still held = "
                  << (g_bytes_in - g_bytes_out) - base << " bytes\n";
    }

    std::cout << "\n=== 3. Reading is FREE: cost only appears when ownership MOVES ===\n";
    {
        constexpr std::size_t R = 8, PASSES = 20'000'000;
        std::vector<std::shared_ptr<Widget>> sh;
        std::vector<Widget*> rp;
        sh.reserve(R);
        rp.reserve(R);
        for (std::size_t i = 0; i < R; ++i) {
            auto sp = std::make_shared<Widget>(static_cast<long>(i));
            rp.push_back(sp.get());
            sh.push_back(std::move(sp));
        }
        long s1 = 0, s2 = 0;
        auto t0 = Clock::now();
        for (std::size_t p = 0; p < PASSES; ++p)
            for (std::size_t i = 0; i < R; ++i)
                s1 += sh[i]->value; // load handle's T*, load value — no count touched
        double ns1 = ns_since(t0) / static_cast<double>(PASSES * R);
        t0 = Clock::now();
        for (std::size_t p = 0; p < PASSES; ++p)
            for (std::size_t i = 0; i < R; ++i)
                s2 += rp[i]->value; // load raw ptr, load value
        double ns2 = ns_since(t0) / static_cast<double>(PASSES * R);
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  through shared_ptr->  : " << ns1 << " ns/read  (checksum " << s1 << ")\n";
        std::cout << "  through raw ptr ->    : " << ns2 << " ns/read  (checksum " << s2 << ")\n";
    }

    std::cout << "\n=== 4. Hand-off cost, ONE thread: who pays when handles move ===\n";
    {
        constexpr std::size_t S = 8, N = 20'000'000;
        auto master = std::make_shared<Widget>();
        std::vector<std::shared_ptr<Widget>> sps(S, master);
        std::vector<std::unique_ptr<Widget>> ups;
        std::vector<Widget*> rps;
        ups.reserve(S);
        rps.reserve(S);
        for (std::size_t i = 0; i < S; ++i) {
            ups.emplace_back(new Widget);
            rps.push_back(ups.back().get());
        }
        double ns = 0.0;
        {
            std::uintptr_t sink = 0;
            auto t0 = Clock::now();
            for (std::size_t i = 0; i < N; ++i) {
                std::shared_ptr<Widget> local = sps[i & (S - 1)]; // copy: atomic add
                sink += reinterpret_cast<std::uintptr_t>(local.get());
            }                                                        // dtor: atomic sub
            ns = ns_since(t0) / static_cast<double>(N);
            std::cout << "  (chk " << sink % 97 << ")\n";
        }
        pns("shared_ptr copy + destroy", ns);
        {
            std::uintptr_t sink = 0;
            auto t0 = Clock::now();
            for (std::size_t i = 0; i < N; ++i) {
                std::unique_ptr<Widget> local = std::move(ups[i & (S - 1)]); // steal
                sink += reinterpret_cast<std::uintptr_t>(local.get());
                ups[i & (S - 1)] = std::move(local); // give back
            }
            ns = ns_since(t0) / static_cast<double>(N);
            std::cout << "  (chk " << sink % 97 << ")\n";
        }
        pns("unique_ptr move out + back", ns);
        {
            std::uintptr_t sink = 0;
            auto t0 = Clock::now();
            for (std::size_t i = 0; i < N; ++i) {
                Widget* local = rps[i & (S - 1)]; // plain load
                sink += reinterpret_cast<std::uintptr_t>(local);
            }
            ns = ns_since(t0) / static_cast<double>(N);
            std::cout << "  (chk " << sink % 97 << ")\n";
        }
        pns("raw pointer copy", ns);
    }

    std::cout << "\n=== 5. TWO threads: instruction tax vs cache-line ping-pong ===\n";
    {
        constexpr std::uint64_t M = 2'000'000;
        std::atomic<bool> go{false};

        // Two workers spin until 'go', then each does M ops. We time from go to
        // both joined: thread startup is NOT part of the measurement.
        auto run_pair = [&](auto&& f1, auto&& f2) -> double {
            std::uint64_t s1 = 0, s2 = 0;
            std::jthread t1([&] { s1 = f1(); });
            std::jthread t2([&] { s2 = f2(); });
            auto t0 = Clock::now();
            go.store(true, std::memory_order_release);
            t1.join();
            t2.join();
            double ns = ns_since(t0) / (2.0 * static_cast<double>(M));
            std::cout << "  (chk " << (s1 + s2) % 97 << ")\n";
            return ns;
        };

        // A: both threads copy the SAME shared_ptr -> ONE control block hammered
        auto shared_target = std::make_shared<Widget>();
        auto copy_shared = [&](std::shared_ptr<Widget> src) -> std::uint64_t {
            while (!go.load(std::memory_order_acquire)) {
            }
            std::uintptr_t sink = 0;
            for (std::uint64_t i = 0; i < M; ++i) {
                std::shared_ptr<Widget> local = src;
                sink += reinterpret_cast<std::uintptr_t>(local.get());
            }
            return static_cast<std::uint64_t>(sink);
        };
        double nsA = run_pair([&] { return copy_shared(shared_target); },
                              [&] { return copy_shared(shared_target); });
        pns("A: both copy the SAME block", nsA);

        // B: each thread copies its OWN shared_ptr -> atomics, no shared line
        auto own1 = std::make_shared<Widget>();
        auto wall = std::make_shared<Chunk>(); // allocation wall between the two hot lines
        auto own2 = std::make_shared<Widget>();
        double nsB = run_pair([&] { return copy_shared(std::move(own1)); },
                              [&] { return copy_shared(std::move(own2)); });
        pns("B: each copies its OWN block", nsB);

        // C: no atomics at all — plain pointer shuffling. The pick is data-dependent
        //    (index from the running checksum) so the optimizer may NOT fold the
        //    loop into a constant — we want the REAL cost of moving plain pointers.
        auto churn_unique = [&](std::vector<std::unique_ptr<Widget>> v) -> std::uint64_t {
            while (!go.load(std::memory_order_acquire)) {
            }
            std::uintptr_t sink = 0;
            for (std::uint64_t i = 0; i < M; ++i) {
                std::size_t j = (sink >> 3) & 7;
                std::unique_ptr<Widget> local = std::move(v[j]);
                sink += reinterpret_cast<std::uintptr_t>(local.get());
                v[j] = std::move(local);
            }
            return static_cast<std::uint64_t>(sink);
        };
        auto make_herd = [] {
            std::vector<std::unique_ptr<Widget>> v;
            v.reserve(8);
            for (int i = 0; i < 8; ++i)
                v.emplace_back(new Widget);
            return v;
        };
        double nsC = run_pair([&] { return churn_unique(make_herd()); },
                              [&] { return churn_unique(make_herd()); });
        pns("C: each moves its unique_ptr", nsC);

        std::cout << "  decomposition:\n";
        std::cout << "    base: plain pointer shuffling   C     = " << std::fixed
                  << std::setprecision(3) << nsC << " ns/op\n";
        std::cout << "    + atomic instruction tax       B - C = " << nsB - nsC << " ns/op\n";
        std::cout << "    + cache-line ping-pong tax     A - B = " << nsA - nsB << " ns/op\n";
        std::cout << "    contended sharing vs no sharing A / C = " << nsA / nsC << "x\n";
    }

    std::cout << "\n=== Verdict ===\n";
    std::cout << "  Reading through any handle: free.\n";
    std::cout << "  MOVING ownership: only unique_ptr (and raw) are free.\n";
    std::cout << "  shared_ptr is not 'better unique_ptr' — it is a different tool,\n";
    std::cout << "  and it bills you every time ownership moves.\n";
    return 0;
}
