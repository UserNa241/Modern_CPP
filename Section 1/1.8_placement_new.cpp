// lesson_1_8_placement_new.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_1_8_placement_new.cpp -o lesson18 && ./lesson18
#include <iostream>
#include <string>
#include <new>       // placement new, std::launder
#include <memory>    // std::destroy_at
#include <cstddef>   // std::byte, offsetof

int g_depth = 0;
struct Tracer {
    const char* name;
    explicit Tracer(const char* n) : name(n) {
        std::cout << std::string(g_depth*2,' ') << "+ " << name << '\n'; ++g_depth;
    }
    ~Tracer() { --g_depth; std::cout << std::string(g_depth*2,' ') << "- " << name << '\n'; }
    void touch() const { std::cout << "      (alive: " << name << ")\n"; }
};

// ================= MiniOptional: optional-in-a-buffer, the real pattern =================
template <class T>
class MiniOptional {
    alignas(T) std::byte buf_[sizeof(T)];   // raw storage, correctly aligned
    bool engaged_ = false;
public:
    MiniOptional() = default;
    ~MiniOptional() { reset(); }                        // law: exactly one destruction

    MiniOptional(const MiniOptional&) = delete;         // (copy/move need real code — §4!)
    MiniOptional& operator=(const MiniOptional&) = delete;

    template <class... A>
    void emplace(A&&... args) {
        if (engaged_) reset();                          // ONE object per storage, ever
        ::new (static_cast<void*>(buf_)) T(args...);    // construct in our bytes
        engaged_ = true;
    }
    void reset() {
        if (engaged_) {
            std::destroy_at(&value());                  // = value().~T(): end lifetime
            engaged_ = false;                           // storage stays OURS
        }
    }
    T&       value()       { return *std::launder(reinterpret_cast<T*>(buf_)); }
    const T& value() const { return *std::launder(reinterpret_cast<const T*>(buf_)); }
    bool     has_value() const { return engaged_; }
};

// ================= the laundering canonical case =================
struct X { const int n; };                               // const member!

int main() {
    std::cout << "=== 1. The mechanic: same bytes, two different lifetimes ===\n";
    alignas(Tracer) std::byte storage[sizeof(Tracer)];
    {
        Tracer* a = ::new (static_cast<void*>(storage)) Tracer("A");
        a->touch();
        a->~Tracer();                        // storage now holds NO object
        Tracer* b = ::new (static_cast<void*>(storage)) Tracer("B");   // SAME bytes, NEW object
        b->touch();
        std::cout << "  a and b point at the SAME address: "
                  << (static_cast<void*>(a) == static_cast<void*>(b)) << '\n';
        b->~Tracer();
    }

    std::cout << "\n=== 2. MiniOptional<Tracer> — optional, from scratch ===\n";
    {
        MiniOptional<Tracer> opt;            // NO Tracer constructed yet!
        std::cout << "  empty: has_value = " << opt.has_value() << '\n';
        opt.emplace("first");
        opt.value().touch();
        opt.emplace("second");               // must destroy 'first' first
        opt.value().touch();
        std::cout << "  leaving scope -> destructor resets:\n";
    }

    std::cout << "\n=== 3. What MiniOptional costs ===\n";
    std::cout << "  sizeof(Tracer) = " << sizeof(Tracer)
              << ",  sizeof(MiniOptional<Tracer>) = " << sizeof(MiniOptional<Tracer>)
              << "   (payload + bool + padding)\n";
    static_assert(sizeof(MiniOptional<Tracer>) >= sizeof(Tracer) + 1);

    std::cout << "\n=== 4. The laundering case: const member, replaced in place ===\n";
    alignas(X) std::byte xbuf[sizeof(X)];
    X* p = ::new (static_cast<void*>(xbuf)) X{1};
    int first = p->n;                        // compiler MAY cache: n is const
    ::new (static_cast<void*>(xbuf)) X{2};   // replace object in SAME storage
    int stale     = p->n;                    // formally UB path (stale pointer)
    int laundered = std::launder(reinterpret_cast<X*>(xbuf))->n;  // blessed path
    std::cout << "  first=" << first << "  stale=" << stale
              << "  laundered=" << laundered << '\n'
              << "  (all three agree HERE — but only 'laundered' is GUARANTEED:\n"
              << "   the compiler is allowed to have cached first into stale.\n"
              << "   Compare Lesson 1.7, where GCC really did exploit the UB.)\n";
    return 0;
}
