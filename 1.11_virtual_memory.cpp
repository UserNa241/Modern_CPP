#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <windows.h>
#include <psapi.h>

using Clock = std::chrono::steady_clock;

long long minflt() {
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	return pmc.PageFaultCount; // Total page faults on Windows
}

long long maxrss_kib() {
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	return pmc.PeakWorkingSetSize / 1024; // Peak physical RAM used
}

// lesson_1_11_virtual_memory.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 lesson_1_11_virtual_memory.cpp -o vm && ./vm

int main() {
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	const long page = si.dwPageSize; // Usually 4096 bytes on Windows
	constexpr std::size_t MiB64 = 64ull * 1024 * 1024;    std::cout << "page size = " << page << " bytes  ->  " << MiB64 / page
              << " pages per 64 MiB buffer\n\n";

    // ---- A: allocate 64 MiB and NEVER touch it ----
    std::cout << "=== A. new char[64MiB], never touched (lazy promise) ===\n";
    long f0 = minflt(), r0 = maxrss_kib();
    auto a = std::make_unique_for_overwrite<char[]>(MiB64);  // default-init: NO zeroing
    std::cout << "  minor faults during alloc: +" << (minflt() - f0)
              << "   RSS grew ~" << (maxrss_kib() - r0) << " KiB\n"
              << "  -> allocation mapped NOTHING yet; it is a promise, not memory\n\n";

    // ---- B: touch ONE byte per page ----
    std::cout << "=== B. write 1 byte per page (first touch) ===\n";
    auto b = std::make_unique_for_overwrite<char[]>(MiB64);
    f0 = minflt();
    auto t0 = Clock::now();
    for (std::size_t i = 0; i < MiB64; i += page) b[i] = 1;
    double ms = std::chrono::duration<double,std::milli>(Clock::now()-t0).count();
    std::cout << "  minor faults: +" << (minflt() - f0) << "   (theory: "
              << MiB64 / page << ")\n"
              << "  time: " << ms << " ms  (~" << ms * 1000.0 / (MiB64/page)
              << " us per fault: kernel building a PTE + zeroed frame each time)\n\n";

    // ---- C: reads fault too (shared zero page), THEN writes fault AGAIN (COW break) ----
    std::cout << "=== C. demand-zero: read faults, then write faults AGAIN ===\n";
    auto c = std::make_unique_for_overwrite<char[]>(MiB64);
    f0 = minflt(); long long sum = 0;
    for (std::size_t i = 0; i < MiB64; i += page) sum += c[i];      // READS
    long read_faults = minflt() - f0;
    f0 = minflt();
    for (std::size_t i = 0; i < MiB64; i += page) c[i] = 2;         // WRITES
    long write_faults = minflt() - f0;
    std::cout << "  read  pass: +" << read_faults
              << "  (kernel maps the ONE shared zero page - no frame allocated)\n"
              << "  write pass: +" << write_faults
              << "  (each page now COPIED to a private zeroed frame)\n"
              << "  reads returned 0 everywhere: sum=" << sum << "\n\n";

    // ---- D: vector reserve vs resize ----
    std::cout << "=== D. std::vector: reserve vs resize ===\n";
    std::vector<char> v;
    f0 = minflt();
    v.reserve(MiB64);
    std::cout << "  reserve(64MiB): +" << (minflt() - f0) << " faults   (capacity only)\n";
    f0 = minflt();
    v.resize(MiB64);
    std::cout << "  resize(64MiB) : +" << (minflt() - f0) << " faults   (value-init ZEROES every page NOW)\n\n";

    // ---- E: the memory model, smallest correct demo ----
    std::cout << "=== E. publish with release, consume with acquire ===\n";
    int payload = 0;                         // plain data...
    std::atomic<bool> ready{false};          // ...guarded by an atomic flag
    std::jthread worker([&] {
        payload = 42;                                       // (1) write payload
        ready.store(true, std::memory_order_release);       // (2) publish: (1) can't sink below
    });
    while (!ready.load(std::memory_order_acquire)) { /* spin */ }  // (3) see flag -> (1) visible
    std::cout << "  payload after acquire-load sees flag: " << payload
              << "   (happens-before: (1) -> (2) -> (3))\n"
              << "  without atomics this is a DATA RACE = UB (not 'slow': meaningless)\n";
    return 0;
}
