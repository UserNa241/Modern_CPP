// lesson_1_10_false_sharing.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 -pthread lesson_1_10_false_sharing.cpp -o fs && ./fs
#include <iostream>
#include <iomanip>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <new>       // hardware_destructive_interference_size
#include <thread>
#include <vector>
#include <algorithm>

using Clock = std::chrono::steady_clock;

template <class F>
double ms_median3(F&& f) {                    // rule 3: median of 3 runs
    double t[3];
    for (auto& x : t) {
        auto start = Clock::now();
        f();
        x = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
    std::sort(t, t + 3);
    return t[1];
}

// ---------- Experiment A: spatial locality ----------
constexpr int ROWS = 4096, COLS = 4096;       // 16.7M ints = 64 MiB
constexpr int N    = 16 * 1024 * 1024;        // 64 MiB flat

long long sum_row_major(const std::vector<int>& m) {
    long long s = 0;
    for (int r = 0; r < ROWS; ++r)
        for (int c = 0; c < COLS; ++c)
            s += m[static_cast<std::size_t>(r) * COLS + c];
    return s;
}
long long sum_col_major(const std::vector<int>& m) {
    long long s = 0;
    for (int c = 0; c < COLS; ++c)
        for (int r = 0; r < ROWS; ++r)
            s += m[static_cast<std::size_t>(r) * COLS + c];
    return s;
}
long long sum_all(const std::vector<int>& v) {
    long long s = 0;
    for (int x : v) s += x;
    return s;
}
long long sum_every_line(const std::vector<int>& v) {   // 1 access per 64-byte line
    long long s = 0;
    for (std::size_t i = 0; i < v.size(); i += 32) s += v[i];
    return s;
}

// ---------- Experiment B: false sharing ----------
struct SharedPair {                            // two independent counters...
    std::atomic<std::uint64_t> a;              // ...one cache line
    std::atomic<std::uint64_t> b;
};
struct alignas(64) PaddedAtomic {              // one counter per line
    std::atomic<std::uint64_t> v;
};
static_assert(sizeof(SharedPair) == 16);
static_assert(sizeof(PaddedAtomic) == 64);

constexpr long ITERS = 30'000'000;

void hammer(std::atomic<std::uint64_t>& c) {
    for (long i = 0; i < ITERS; ++i)
        c.fetch_add(1, std::memory_order_relaxed);      // per-thread counter: no ordering needed
}

template <class PairLike>
double run_two_threads(PairLike& data) {
    auto f = [&] {
        std::jthread t0(hammer, std::ref(data.a));
        std::jthread t1(hammer, std::ref(data.b));
    };                                                  // js join here
    return ms_median3(f);
}

int main() {
    std::cout << std::fixed << std::setprecision(2);

    std::cout << "=== 0. Geometry ===\n";
    std::cout << "  cache line (hardware_destructive_interference_size) = "
              << std::hardware_destructive_interference_size << " bytes\n";
    SharedPair sp{};
    PaddedAtomic pad[2]{};
    std::cout << "  SharedPair : &a=" << &sp.a << "  &b=" << &sp.b
              << "  (delta " << reinterpret_cast<char*>(&sp.b) - reinterpret_cast<char*>(&sp.a)
              << " B -> SAME line)\n";
    std::cout << "  Padded[2]  : delta "
              << reinterpret_cast<char*>(&pad[1].v) - reinterpret_cast<char*>(&pad[0].v)
              << " B -> SEPARATE lines\n";

    std::cout << "\n=== A. Spatial locality (64 MiB matrix) ===\n";
    std::vector<int> m(ROWS * COLS);
    for (std::size_t i = 0; i < m.size(); ++i) m[i] = static_cast<int>(i % 7);
    long long sink = 0;                       // RULE 4: consume results or the loops DIE
    double row = ms_median3([&] { sink += sum_row_major(m); });
    double col = ms_median3([&] { sink += sum_col_major(m); });
    std::cout << "  row-major    : " << std::setw(8) << row << " ms   (streaming: prefetcher on)\n"
              << "  column-major : " << std::setw(8) << col << " ms   (16 KiB stride each step)\n"
              << "  ratio        : " << col / row << "x slower for the SAME work\n";

    std::cout << "\n=== A2. You pay per LINE, not per byte (64 MiB vector) ===\n";
    std::vector<int> v(N);
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<int>(i % 5);
    double full = ms_median3([&] { sink += sum_all(v); });
    double strd = ms_median3([&] { sink += sum_every_line(v); });
    std::cout << "  sum ALL 16.7M values : " << std::setw(8) << full << " ms\n"
              << "  sum 1/16 (per line)  : " << std::setw(8) << strd << " ms\n"
              << "  -> 1/16 of the data, " << strd / full * 100 << "% of the time\n"
              << "     (the other 15 values per line were fetched and ignored)\n"
              << "  [loops kept alive: sink = " << sink << "]\n";

    std::cout << "\n=== B. False sharing (2 threads, 2 cores, 30M increments each) ===\n";
    SharedPair shared{};
    double t_shared = run_two_threads(shared);
    struct PaddedPair {                       // each member on its own line
        alignas(64) std::atomic<std::uint64_t> a;
        alignas(64) std::atomic<std::uint64_t> b;
    } padded{};
    static_assert(sizeof(PaddedPair) == 128);
    double t_padded = run_two_threads(padded);
    std::cout << "  same cache line : " << std::setw(8) << t_shared << " ms\n"
              << "  padded (64 B)   : " << std::setw(8) << t_padded << " ms\n"
              << "  speedup         : " << t_shared / t_padded << "x  for identical work\n"
              << "  (90M per counter = 30M x 3 median runs — benchmark ran 3x)\n";
    return 0;
}
