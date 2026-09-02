// lesson_2_2_rule_of_five.cpp — portable, standard C++ only
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_2_2_rule_of_five.cpp -o r5 && ./r5
#include <iostream>
#include <cstddef>
#include <utility>
#include <vector>

// ================= Rule of THREE: destructor + deep copy + deep assign =================
class RawThree {
    int* p_ = nullptr; std::size_t n_ = 0;
public:
    explicit RawThree(std::size_t n) : p_(new int[n]{}), n_(n)
        { std::cout << "  [alloc " << n_ << "]\n"; }
    ~RawThree() { std::cout << "  [free " << n_ << "]\n"; delete[] p_; }

    RawThree(const RawThree& o) : p_(new int[o.n_]{}), n_(o.n_) {          // deep copy
        for (std::size_t i = 0; i < n_; ++i) p_[i] = o.p_[i];
        std::cout << "  [deep-copy " << n_ << "]\n";
    }
    RawThree& operator=(const RawThree& o) {                               // self-safe
        if (this != &o) {
            int* fresh = new int[o.n_]{};
            for (std::size_t i = 0; i < o.n_; ++i) fresh[i] = o.p_[i];
            delete[] p_;  p_ = fresh;  n_ = o.n_;
        }
        std::cout << "  [copy-assign " << n_ << "]\n";  return *this;
    }
    // NOTE: no move members. The suppression table says: implicit moves GONE.
};

// ================= Rule of FIVE: Three + steal operations =================
class RawFive {
    int* p_ = nullptr; std::size_t n_ = 0;
public:
    explicit RawFive(std::size_t n) : p_(new int[n]{}), n_(n)
        { std::cout << "  [alloc " << n_ << "]\n"; }
    ~RawFive() { std::cout << "  [free " << n_ << "]\n"; delete[] p_; }

    RawFive(const RawFive& o) : p_(new int[o.n_]{}), n_(o.n_) {
        for (std::size_t i = 0; i < n_; ++i) p_[i] = o.p_[i];
        std::cout << "  [deep-copy " << n_ << "]\n";
    }
    RawFive& operator=(const RawFive& o) {
        if (this != &o) {
            int* fresh = new int[o.n_]{};
            for (std::size_t i = 0; i < o.n_; ++i) fresh[i] = o.p_[i];
            delete[] p_;  p_ = fresh;  n_ = o.n_;
        }
        std::cout << "  [copy-assign " << n_ << "]\n";  return *this;
    }
    RawFive(RawFive&& o) noexcept : p_(o.p_), n_(o.n_) {          // STEAL
        o.p_ = nullptr; o.n_ = 0;                                  // source -> empty husk
        std::cout << "  [steal " << n_ << "]\n";
    }
    RawFive& operator=(RawFive&& o) noexcept {
        if (this != &o) { delete[] p_; p_ = o.p_; n_ = o.n_; o.p_ = nullptr; o.n_ = 0; }
        std::cout << "  [move-assign " << n_ << "]\n";  return *this;
    }
};

// ================= Rule of ZERO: no special members at all =================
class ZBuf {
    std::vector<int> v_;                        // vector IS a tested Rule-of-Five class
public:
    explicit ZBuf(std::size_t n) : v_(n) { std::cout << "  [zero: alloc " << v_.size() << "]\n"; }
    // no dtor, no copy, no move. Nothing. The defaults are memberwise -> correct.
};

int main() {
    std::cout << "=== 1. Rule of Three: copies are deep and safe ===\n";
    {
        RawThree a(3);
        RawThree b = a;              // deep copy
        b = a;                       // copy assign
        b = b;                       // self-assignment: guarded, safe
    }                                // two frees, no double free
    std::cout << "\n=== 2. THE TRAP: std::move on a Three-only class ===\n";
    {
        RawThree a(3);
        RawThree b = std::move(a);   // moves suppressed -> falls back to COPY.
        std::cout << "  ^ that says deep-copy, not steal: the move was SILENTLY a copy\n";
    }
    std::cout << "\n=== 3. Rule of Five: expiring objects get robbed ===\n";
    {
        RawFive a(5);
        RawFive b = std::move(a);    // steal: no allocation at all
        std::cout << "  a is now an empty husk (n=0), b owns the 5 ints\n";
        RawFive c(2);
        c = b;            // move-assign: c frees its 2, steals b's 5
    }
    std::cout << "\n=== 4. Rule of Zero: same job, zero special members ===\n";
    {
        ZBuf x(10);
        ZBuf y = x;                  // copies: correct (vector deep-copies)
        ZBuf z = std::move(x);       // moves: correct (vector steals) - cheap
        std::cout << "  copied AND moved with no code written: members do the work\n";
    }
    return 0;
}
