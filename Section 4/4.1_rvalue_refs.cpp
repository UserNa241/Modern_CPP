// lesson_4_1_rvalue_refs.cpp — the move revolution's missing primitive.
//
// ONE idea: C++98 could not say "this value is expiring — drain it."
// T&& is the language's answer: a reference that binds ONLY to expiring
// values, giving overload resolution a new dimension to route on.
//
//   1. overload pickup: which picks() fires for named / const / temporary /
//      moved / const-moved values
//   2. the copy era vs the move era: real std::vector growth, counted
//   3. handing a value over: 1998 copies because it can't say "take it"
//   4. the first move-only type: exclusive ownership becomes expressible
//
// Build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_4_1_rvalue_refs.cpp -o rv && ./rv
// MSVC : cl /std:c++20 /EHsc /W4 lesson_4_1_rvalue_refs.cpp

#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

struct Stats {
    static inline long ctors = 0, copies = 0, moves = 0;
    static inline long bytes_copied = 0;    // payload physically duplicated
    static inline long bytes_rescued = 0;   // payload a steal avoided copying
    static void reset() { ctors = copies = moves = bytes_copied = bytes_rescued = 0; }
};

// ---------------------------------------- the copy-era type
// Declaring the copy ctor suppresses the implicit move ctor (2.2's table):
// this is exactly what C++98 gave us — deep copies, always, everywhere.
struct OldBuffer {
    static constexpr std::size_t PAYLOAD = 1024;
    char* data_;
    OldBuffer() : data_(new char[PAYLOAD]) { ++Stats::ctors; }
    ~OldBuffer() { delete[] data_; }
    OldBuffer(const OldBuffer&) : data_(new char[PAYLOAD]) {
        ++Stats::copies;
        Stats::bytes_copied += PAYLOAD;
    }
};

// ---------------------------------------- the move-era type: same shape + T&& ctor
struct NewBuffer {
    static constexpr std::size_t PAYLOAD = 1024;
    char* data_;
    NewBuffer() : data_(new char[PAYLOAD]) { ++Stats::ctors; }
    ~NewBuffer() { delete[] data_; }
    NewBuffer(const NewBuffer&) : data_(new char[PAYLOAD]) {
        ++Stats::copies;
        Stats::bytes_copied += PAYLOAD;
    }
    // The rvalue-reference constructor: the argument is EXPIRING, so we may
    // steal (2.8's recipe: steal -> empty the source -> noexcept).
    NewBuffer(NewBuffer&& o) noexcept : data_(o.data_) {
        o.data_ = nullptr;
        ++Stats::moves;
        Stats::bytes_rescued += PAYLOAD;
    }
};

// ---------------------------------------- 1. overload pickup
struct Tag {};
const char* picks(Tag&)       { return "Tag&       (identity, mutable)"; }
const char* picks(const Tag&) { return "const Tag& (identity, read-only — 1998's universal sink)"; }
const char* picks(Tag&&)      { return "Tag&&      (expiring: nobody can observe it again)"; }

Tag make_tag() { return Tag{}; }   // returns by value -> an rvalue at the call site

void demo_pickup() {
    std::cout << "=== 1. Overload pickup ===\n";
    Tag t;
    const Tag ct;
    std::cout << "  picks(t)             -> " << picks(t) << "\n";
    std::cout << "  picks(ct)            -> " << picks(ct) << "\n";
    std::cout << "  picks(Tag{})         -> " << picks(Tag{}) << "\n";
    std::cout << "  picks(make_tag())    -> " << picks(make_tag()) << "\n";
    std::cout << "  picks(std::move(t))  -> " << picks(std::move(t))
              << "   <- a relabel, not a move (4.2)\n";
    std::cout << "  picks(std::move(ct)) -> " << picks(std::move(ct))
              << "   <- const rvalue: T&& refuses it\n";
    std::cout << "  picks(t) again       -> " << picks(t)
              << "   (t untouched — the cast changed nothing)\n";
}

// ---------------------------------------- 2. growth: copy era vs move era
void demo_growth() {
    std::cout << "\n=== 2. std::vector growth, 10 pushes, 1024-byte payloads ===\n";
    Stats::reset();
    {
        std::vector<OldBuffer> v;
        for (int i = 0; i < 10; ++i) v.push_back(OldBuffer{});
    }
    std::cout << "  copy era (no move ctor): copies=" << Stats::copies
              << ", bytes physically copied=" << Stats::bytes_copied << "\n";
    Stats::reset();
    {
        std::vector<NewBuffer> v;
        for (int i = 0; i < 10; ++i) v.push_back(NewBuffer{});
    }
    std::cout << "  move era (T&& ctor)   : moves=" << Stats::moves
              << ", copies=" << Stats::copies
              << ", payload rescued=" << Stats::bytes_rescued << " bytes\n";
}

// ---------------------------------------- 3. handing a value over
void sink_old(OldBuffer b) { (void)b; }   // by value: the call constructs b
void sink_new(NewBuffer b) { (void)b; }   // copy-constructs from lvalues,
                                          // move-constructs from rvalues

void demo_handover() {
    std::cout << "\n=== 3. Handing a value over: 1998 cannot say 'take it' ===\n";
    Stats::reset();
    {
        OldBuffer o;
        sink_old(o);            // we are DONE with o — 1998 copies anyway
        std::cout << "  sink_old(o)            : copies=" << Stats::copies
                  << " (" << Stats::bytes_copied << " bytes duplicated)\n";
    }
    Stats::reset();
    {
        NewBuffer n;
        sink_new(n);            // lvalue WITHOUT std::move: nothing said
        std::cout << "  sink_new(n)            : copies=" << Stats::copies
                  << "  <- lvalue: the T&& ctor is not even a candidate\n";
        Stats::reset();
        sink_new(std::move(n)); // relabel to rvalue: the T&& ctor fires
        std::cout << "  sink_new(std::move(n)) : moves=" << Stats::moves
                  << ", copies=" << Stats::copies << " (0 bytes duplicated)\n";
    }
    // And the case we deliberately did NOT demo: sink_new(NewBuffer{}) builds
    // the argument DIRECTLY IN the parameter — no move, no copy, the object
    // simply materializes there. That is (guaranteed) elision — Lesson 4.4.
}

// ---------------------------------------- 4. the first move-only type
struct FileHandle {
    int fd;
    static inline long opened = 0, closed = 0;
    explicit FileHandle(int f) : fd(f) { ++opened; }
    ~FileHandle() { if (fd != -1) ++closed; }
    FileHandle(const FileHandle&) = delete;             // exclusive: no copies
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&& o) noexcept : fd(o.fd) { o.fd = -1; }
    FileHandle& operator=(FileHandle&& o) noexcept {    // 3.4: the order is the guard
        int old = fd;      // save
        fd = o.fd;         // take
        o.fd = -1;         // empty the source
        if (old != -1) ++closed;
        return *this;
    }
};

void demo_move_only() {
    std::cout << "\n=== 4. Exclusive ownership becomes EXPRESSIBLE ===\n";
    {
        std::vector<FileHandle> v;
        for (int fd = 3; fd <= 5; ++fd) v.push_back(FileHandle(fd));
        std::cout << "  3 handles in a vector: opened=" << FileHandle::opened
                  << ", closed-so-far=" << FileHandle::closed
                  << "  (growth MOVED them, no fd dropped)\n";
        // FileHandle copy = v[0];        // compile error: copy deleted
        // std::vector<FileHandle> w = v; // compile error: vector copy needs
        //                                  element copies — 1998: impossible type
    }
    std::cout << "  scope closed: closed=" << FileHandle::closed
              << " == opened  (each handle released exactly once)\n";
}

int main() {
    demo_pickup();
    demo_growth();
    demo_handover();
    demo_move_only();
    return 0;
}
