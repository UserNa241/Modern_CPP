// lesson_2_1_raii.cpp — RAII with plain, portable C++ (works on Windows/Linux/mac)
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_2_1_raii.cpp -o raii && ./raii
#include <iostream>
#include <stdexcept>
#include <cstddef>

// The resource here is HEAP MEMORY (new[]/delete[]) — 100% standard C++.
// The judge is a simple counter: how many resources exist right now?
class Buffer {
public:
    static int live;                                   // resources alive right now

    explicit Buffer(std::size_t n) : data_(new int[n]{}), size_(n) {
        if (n == 0) { delete[] data_; throw std::invalid_argument("empty buffer"); }
        ++live;
        std::cout << "  [acquire] " << size_ << " ints\n";
    }
    ~Buffer() {
        --live;
        delete[] data_;                                // the ONE cleanup line, runs on EVERY path
        std::cout << "  [release] " << size_ << " ints\n";
    }
    Buffer(const Buffer&)            = delete;         // copying would double-delete
    Buffer& operator=(const Buffer&) = delete;

    void  fill(int v)       { for (std::size_t i = 0; i < size_; ++i) data_[i] = v; }
    long  sum() const       { long s = 0; for (std::size_t i = 0; i < size_; ++i) s += data_[i]; return s; }

private:
    int* data_;
    std::size_t size_;
};
int Buffer::live = 0;

// ---------- the OLD way: cleanup by hand, on every path ----------
int manual_style(bool fail) {
    int* p = new int[1000];                // acquired...
    p[0] = 7;
    if (fail) return -1;                   // ...and this early exit forgets delete[] -> LEAK
    delete[] p;
    return 0;
}

// ---------- the RAII way: zero cleanup lines ----------
int raii_style(bool fail) {
    Buffer b(1000);                        // acquired (and owned by b)
    b.fill(7);
    if (fail) return -1;                   // destructor closes it. Automatically.
    return 0;
}

void raii_with_exception() {
    Buffer b(10);
    throw std::runtime_error("disk exploded");
}

int main() {
    std::cout << "1. normal scope\n";
    {
        Buffer b(5);
        b.fill(3);
        std::cout << "  sum = " << b.sum() << ", live = " << Buffer::live << "\n";
    }
    std::cout << "  after scope: live = " << Buffer::live << "  (auto-released)\n\n";

    std::cout << "2. manual style, early return\n";
    manual_style(true);
    std::cout << "  (no crash, no message - the 4000 bytes are just GONE; AddressSanitizer will name it)\n\n";

    std::cout << "3. RAII, early return\n";
    raii_style(true);
    std::cout << "  live = " << Buffer::live << "  (clean, zero cleanup code)\n\n";

    std::cout << "4. RAII, exception\n";
    try { raii_with_exception(); }
    catch (const std::exception& e) { std::cout << "  caught: " << e.what() << "\n"; }
    std::cout << "  live = " << Buffer::live << "  (release printed BEFORE 'caught' - dtor ran during unwinding)\n\n";

    std::cout << "5. failed acquisition\n";
    try { Buffer bad(0); }
    catch (const std::invalid_argument& e) {
        std::cout << "  ctor threw: " << e.what() << " - live = " << Buffer::live
                  << " (no object existed, nothing to clean)\n";
    }
    return 0;
}