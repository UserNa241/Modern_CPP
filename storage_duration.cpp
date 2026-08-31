// lesson_1_1_storage_duration.cpp — observe all four storage durations
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_1_1_storage_duration.cpp -o lesson11 && ./lesson11
#include <iostream>
#include <thread>     // jthread
#include <cstdint>    // uintptr_t

// ---------- STATIC STORAGE ----------
int g_initialized = 42;   // non-zero initializer  -> .data
int g_zero;               // no initializer        -> .bss (guaranteed 0)
int g_zero_too = 0;       // all-zero initializer  -> .bss as well
char big[100'000'000];

// Instrumented class: logs its own birth and death
struct Tracer {
    const char* name;
    explicit Tracer(const char* n) : name(n) {
        std::cout << "      [constructed] " << name << '\n';
    }
    ~Tracer() {
        std::cout << "      [destroyed ] " << name << '\n';
    }
};

Tracer g_tracer("global Tracer (static storage)");  // built BEFORE main

// ---------- THREAD STORAGE ----------
thread_local int t_counter = 0;   // one private copy per thread

void nested_frame() {
    int local = 7;                    // automatic, in nested frame
    static int s_local = 10;          // static, but block scope
    int* heap = new int(99);          // dynamic

    std::cout << "  nested_frame():\n"
              << "    stack (automatic) : " << &local
              << "   <- LOWER than main's stack\n"
              << "    static (local)    : " << &s_local
              << "   <- same region as globals\n"
              << "    heap (dynamic)    : " << heap << '\n';
    delete heap;
}

int main() {
    std::cout << std::boolalpha;

    std::cout << "=== 1. One variable per storage region ===\n";
    int a = 1;   // automatic
    int b = 2;   // automatic, declared after a
    std::cout << "  .data  &g_initialized : " << &g_initialized << '\n'
              << "  .bss   &g_zero        : " << &g_zero
              << "  (value = " << g_zero << ", zero for free)\n"
              << "  .bss   &g_zero_too    : " << &g_zero_too << '\n'
              << "  stack  &a             : " << &a << '\n'
              << "  stack  &b             : " << &b
              << "  (b < a ? " << (reinterpret_cast<std::uintptr_t>(&b)
                                 < reinterpret_cast<std::uintptr_t>(&a))
              << "  -> stack grows DOWN)\n";
    nested_frame();

    std::cout << "\n=== 2. Heap grows UP ===\n";
    int* h1 = new int(1);
    int* h2 = new int(2);
    std::cout << "  h1 = " << h1 << "\n  h2 = " << h2
              << "  (h2 > h1 ? " << (reinterpret_cast<std::uintptr_t>(h2)
                                    > reinterpret_cast<std::uintptr_t>(h1))
              << ")\n";
    delete h1;
    delete h2;

    std::cout << "\n=== 3. thread_local: one copy PER THREAD ===\n";
    ++t_counter;                       // main thread's copy
    std::cout << "  main   thread: t_counter = " << t_counter
              << "  @ " << &t_counter << '\n';
    std::jthread worker([] {
        ++t_counter;                   // worker's OWN copy
        ++t_counter;
        std::cout << "  worker thread: t_counter = " << t_counter
                  << "  @ " << &t_counter
                  << "   <- different object AND address\n";
    });                                // jthread joins automatically

    std::cout << "  after worker joined, main's t_counter still = "
              << t_counter << '\n';

    std::cout << "\n=== 4. Destruction order is REVERSE of construction ===\n";
    {
        Tracer first("block: first");
        Tracer second("block: second");
        std::cout << "      ...block ends...\n";
    }

    std::cout << "\n=== 5. Local static: initialized exactly ONCE ===\n";
    for (int i = 1; i <= 3; ++i) {
        Tracer once("local static Tracer");
        (void)once;
        std::cout << "      loop pass " << i << '\n';
    }

    std::cout << "\nmain() returns now — static destructors run AFTER this:\n";
	system("pause");
    return 0;
}