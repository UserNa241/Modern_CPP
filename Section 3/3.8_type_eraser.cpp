// lesson_3_8_type_erasure.cpp — Phase C: type-erased deleters & allocate_shared.
//
// The question this lesson answers: my::shared_ptr<Widget> is ONE type. How can
// that one type destroy its Widget with a plain delete, OR a fat stateful
// deleter, OR a session-logging lambda — without the deleter appearing in the
// handle's template parameters, and without the handle growing a byte?
//
// The answer is TYPE ERASURE, and you have already built it twice without the
// name: every ControlBlock since 3.5 hides its real type behind one base
// pointer. Today we finish the machine:
//     CountedErased<T, D>  — ANY deleter, stored inside the block
//     allocate_shared      — ANY allocator, the block allocated from IT
// and we measure everything: block sizes, allocation counts, std::function's
// small-buffer trick, and the true runtime price of an erased call.
//
// Build: g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror lesson_3_8_type_erasure.cpp -o te && ./te
// Stamp: g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined lesson_3_8_type_erasure.cpp -o te_san && ./te_san
// MSVC : cl /std:c++20 /EHsc /W4 /O2 lesson_3_8_type_erasure.cpp

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// =================================================== instrumented allocator
std::size_t g_allocs = 0, g_frees = 0, g_fail_at = 0;

void* operator new(std::size_t n) {
    if (g_fail_at != 0 && g_allocs + 1 == g_fail_at) {
        g_fail_at = 0;
        throw std::bad_alloc{};
    }
    ++g_allocs;
    if (void* p = std::malloc(n))
        return p;
    throw std::bad_alloc{};
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
void operator delete(void* p) noexcept { ++g_frees; std::free(p); }
void operator delete(void* p, std::size_t) noexcept { ++g_frees; std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

// =================================================== check harness
int g_pass = 0, g_fail = 0, g_section_pass = 0;
const char* g_section = "";
#define CHECK(cond, msg)                                                 \
    do {                                                                 \
        if (cond) { ++g_pass; ++g_section_pass; }                        \
        else { ++g_fail; std::cout << "    FAIL: " << msg << "\n"; }     \
    } while (0)
#define SECTION(name)                                                    \
    do {                                                                 \
        std::cout << "=== " << name << " ===\n";                         \
        g_section = name;                                                \
        g_section_pass = 0;                                              \
    } while (0)
#define SECTION_END() std::cout << "    -> " << g_section_pass << " checks passed\n"

// =================================================== the library (3.7 core + erasure)
namespace my {
namespace detail {

inline bool g_quiet = false;
inline std::int64_t g_blocks_born = 0, g_blocks_gone = 0;

class ControlBlock {
public:
    ControlBlock() noexcept {
        ++g_blocks_born;
        if (!g_quiet) std::cout << "    [block " << static_cast<const void*>(this) << " born]\n";
    }
    void add_strong() noexcept { strong_.fetch_add(1, std::memory_order_relaxed); }
    void add_weak()   noexcept { weak_.fetch_add(1, std::memory_order_relaxed); }

    bool try_add_strong() noexcept {                       // lock()'s CAS engine
        auto n = strong_.load(std::memory_order_relaxed);
        while (n != 0) {
            if (strong_.compare_exchange_weak(n, n + 1,
                                              std::memory_order_acq_rel,
                                              std::memory_order_relaxed))
                return true;
        }
        return false;
    }
    void release_strong() noexcept {
        auto old = strong_.fetch_sub(1, std::memory_order_acq_rel);
        if (old == 1) {
            dispose();        // phase 1: destroy T (the erased call!)
            release_weak();   // the seat
        }
    }
    void release_weak() noexcept {
        auto old = weak_.fetch_sub(1, std::memory_order_acq_rel);
        if (old == 1)
            destroy();        // phase 2: free the block
    }
    std::int32_t use_count() const noexcept {
        return strong_.load(std::memory_order_relaxed);
    }

    virtual const void* tag() const noexcept {
        return nullptr;
    }

protected:
    void gone() noexcept { ++g_blocks_gone; }
    ~ControlBlock() = default;

private:
    virtual void dispose() noexcept = 0;
    virtual void destroy() noexcept = 0;

    std::atomic<std::int32_t> strong_{1};
    std::atomic<std::int32_t> weak_{1};
};

// ---- layout 1: block and T apart, plain delete
template <class T>
class CountedSeparate final : public ControlBlock {
public:
    explicit CountedSeparate(T* p) noexcept : p_(p) {}
private:
    void dispose() noexcept override { delete p_; }
    void destroy() noexcept override { gone(); delete this; }
    T* p_;
};

// ---- layout 2: [block | T] in one chunk (make_shared)
template <class T>
class CountedInPlace final : public ControlBlock {
public:
    template <class... A>
    explicit CountedInPlace(A&&... a) {
        try {
            ::new (static_cast<void*>(buf_)) T(std::forward<A>(a)...);
        } catch (...) {
            gone();     // 2.9: the new-expression frees the chunk
            throw;      // RETHROW ONLY — deleting here would be a double free
        }
    }
    T* value() noexcept { return reinterpret_cast<T*>(buf_); }
private:
    void dispose() noexcept override { value()->~T(); }
    void destroy() noexcept override {
        gone();
        ::operator delete(this, sizeof(CountedInPlace));
    }
    alignas(T) std::byte buf_[sizeof(T)];
};

// ---- layout 3: THE ERASED BLOCK — any deleter D, stored inside the block.
//      The type D exists HERE, at compile time, and nowhere else. The handle
//      will hold this object through a ControlBlock* and never learn what D was.
//      The deleter's captured STATE (session ids, log pointers, pool pointers)
//      rides in the chunk, inline — the block IS the small buffer.
template <class T, class D>
class CountedErased final : public ControlBlock {
public:
    static inline char tag_{};
    CountedErased(T* p, D d) noexcept(std::is_nothrow_move_constructible_v<D>)
        : p_(p), d_(std::move(d)) {}

    const void* tag() const noexcept override {
        return &tag_;
    }

    D* get_d() noexcept {
        return &d_;
    }
private:
    void dispose() noexcept override {
        d_(p_);          // the ERASED CALL: one vtable hop, then the deleter runs
    }                    // contract (std's too): the deleter must not throw
    void destroy() noexcept override {
        gone();
        delete this;     // ~CountedErased destroys d_ (its captures) for us
    }
    T* p_;
    [[no_unique_address]] D d_{};   // empty deleter -> zero chunk growth
};

// ---- layout 4: allocate_shared — [block | T] in one chunk, BOTH allocated
//      from the caller's allocator A (rebound to the block type).
template <class T, class A>
class CountedInPlaceAlloc final : public ControlBlock {
public:
    template <class... Args>
    CountedInPlaceAlloc(const A& a, Args&&... args) : a_(a) {
        try {
            ::new (static_cast<void*>(buf_)) T(std::forward<Args>(args)...);
        } catch (...) {
            gone();     // balance the base's ++born before the caller deallocates
            throw;      // caller (allocate_shared) returns the raw chunk to A
        }
    }
    T* value() noexcept { return reinterpret_cast<T*>(buf_); }
private:
    void dispose() noexcept override { value()->~T(); }
    void destroy() noexcept override {
        gone();
        // The allocator is copied OUT before the memory it lives in is returned
        // (std does the same dance: you cannot use an object to free itself).
        typename std::allocator_traits<A>::template rebind_alloc<CountedInPlaceAlloc> ba(a_);
        ba.deallocate(this, 1);
    }
    A a_;               // the allocator rides in the block TOO — also erased
    alignas(T) std::byte buf_[sizeof(T)];
};

}  // namespace detail

template <class T> class weak_ptr;
template <class T> class enable_shared_from_this;

template <class T>
class shared_ptr {
public:
    shared_ptr() noexcept : p_(nullptr), blk_(nullptr) {}

    explicit shared_ptr(T* p) : p_(p), blk_(nullptr) {
        try {
            blk_ = new detail::CountedSeparate<T>(p);
        } catch (...) {
            delete p;        // 3.7 fix 1: ownership accepted at entry
            throw;
        }
        board_self();
    }

    // THE ERASING CONSTRUCTOR: D is a template parameter HERE and nowhere
    // else. This one line is where the type system builds the specialized
    // machine (CountedErased<T,D>) and the pointer assignment throws away
    // the blueprint (upcast to ControlBlock*).
    //
    // The requires-clause is load-bearing, and this file's first build failed
    // without it: allocate_shared constructs handles internally as
    // shared_ptr(p, blk) with blk of DERIVED block type. The pair-ctor below
    // needs a derived->base conversion, this template deduces D exactly ->
    // the template would WIN and try to "call" a control block as a deleter.
    // (lock() was never at risk: it passes ControlBlock* exactly, and a tie
    // goes to the non-template.) std::shared_ptr constrains its template
    // ctors for exactly this reason. A template ctor never hijacks a COPY —
    // but it hijacks everything it matches BETTER.
    template <class D>
    requires (!std::is_base_of_v<detail::ControlBlock, std::remove_pointer_t<D>>)
    shared_ptr(T* p, D d) : p_(p), blk_(nullptr) {
        try {
            blk_ = new detail::CountedErased<T, D>(p, std::move(d));
        } catch (...) {
            d(p);            // OOM: operator new threw BEFORE the move — d is intact
            throw;           // (a throwing D move ctor is pathological; std requires
        }                    //  deleters not to throw — see the honesty table)
        board_self();
    }

    template<class D>
    D* get_deleter() const noexcept {
        // 1. Does the block exist?
        // 2. Does the block's fingerprint match the fingerprint of CountedErased<T, D>?
        if (blk_ && blk_->tag() == &detail::CountedErased<T,D>::tag_) {
            // 3. Perfect match! We can safely cast it and get the deleter.
            auto* erased_blk = static_cast<detail::CountedErased<T, D>*>(blk_);
            return erased_blk->get_d();
        }
        return nullptr;
    }

    shared_ptr(const shared_ptr& o) noexcept : p_(o.p_), blk_(o.blk_) {
        if (blk_) blk_->add_strong();
    }
    shared_ptr(shared_ptr&& o) noexcept : p_(o.p_), blk_(o.blk_) {
        o.p_ = nullptr;
        o.blk_ = nullptr;
    }
    ~shared_ptr() { if (blk_) blk_->release_strong(); }

    shared_ptr& operator=(const shared_ptr& o) noexcept {
        shared_ptr tmp(o);
        swap(tmp);
        return *this;
    }
    shared_ptr& operator=(shared_ptr&& o) noexcept {
        shared_ptr tmp(std::move(o));
        swap(tmp);
        return *this;
    }

    template <class U>
    shared_ptr(const shared_ptr<U>& owner, T* p) noexcept : p_(p), blk_(owner.blk_) {
        if (blk_) blk_->add_strong();
    }

    T& operator*() const noexcept { return *p_; }
    T* operator->() const noexcept { return p_; }
    T* get() const noexcept { return p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }
    long use_count() const noexcept { return blk_ ? blk_->use_count() : 0; }

    void reset() noexcept {
        shared_ptr tmp;
        swap(tmp);
    }
    void swap(shared_ptr& o) noexcept {
        std::swap(p_, o.p_);
        std::swap(blk_, o.blk_);
    }

private:
    shared_ptr(T* p, detail::ControlBlock* b) noexcept : p_(p), blk_(b) {}

    void board_self() {
        if constexpr (std::is_base_of_v<enable_shared_from_this<T>, T>) {
            if (p_ && p_->weak_this_.blk_ == nullptr)
                p_->weak_this_ = *this;
        }
    }

    template <class U, class... A> friend shared_ptr<U> make_shared(A&&...);
    template <class U, class ALLOC, class... A>
    friend shared_ptr<U> allocate_shared(const ALLOC&, A&&...);
    template <class U> friend class weak_ptr;
    template <class U> friend class shared_ptr;

    T* p_;
    detail::ControlBlock* blk_;
};

template <class T>
class weak_ptr {
public:
    weak_ptr() noexcept : p_(nullptr), blk_(nullptr) {}
    weak_ptr(const shared_ptr<T>& s) noexcept : p_(s.p_), blk_(s.blk_) {
        if (blk_) blk_->add_weak();
    }
    weak_ptr(const weak_ptr& o) noexcept : p_(o.p_), blk_(o.blk_) {
        if (blk_) blk_->add_weak();
    }
    weak_ptr(weak_ptr&& o) noexcept : p_(o.p_), blk_(o.blk_) {
        o.p_ = nullptr;
        o.blk_ = nullptr;
    }
    ~weak_ptr() { if (blk_) blk_->release_weak(); }
    weak_ptr& operator=(const weak_ptr& o) {
        weak_ptr tmp(o);
        swap(tmp);
        return *this;
    }
    weak_ptr& operator=(weak_ptr&& o) noexcept {
        weak_ptr tmp(std::move(o));
        swap(tmp);
        return *this;
    }
    bool expired() const noexcept { return !blk_ || blk_->use_count() == 0; }
    long use_count() const noexcept { return blk_ ? blk_->use_count() : 0; }
    shared_ptr<T> lock() const noexcept {
        if (blk_ && blk_->try_add_strong())
            return shared_ptr<T>(p_, blk_);
        return shared_ptr<T>();
    }
    void reset() noexcept {
        weak_ptr tmp;
        swap(tmp);
    }
    void swap(weak_ptr& o) noexcept {
        std::swap(p_, o.p_);
        std::swap(blk_, o.blk_);
    }
    T* p_;
    detail::ControlBlock* blk_;
};

template <class T>
class enable_shared_from_this {
protected:
    enable_shared_from_this() = default;
    enable_shared_from_this(const enable_shared_from_this&) = default;
    enable_shared_from_this& operator=(const enable_shared_from_this&) noexcept {
        return *this;                        // identity is not part of the value
    }
    ~enable_shared_from_this() = default;
public:
    shared_ptr<T> shared_from_this() { return weak_this_.lock(); }
    weak_ptr<T> weak_from_this() const noexcept { return weak_this_; }
private:
    weak_ptr<T> weak_this_;
    template <class U> friend class shared_ptr;
};

template <class T, class... A>
[[nodiscard]] shared_ptr<T> make_shared(A&&... a) {
    auto* blk = new detail::CountedInPlace<T>(std::forward<A>(a)...);
    shared_ptr<T> sp(blk->value(), blk);
    sp.board_self();
    return sp;
}

// allocate_shared: make_shared's one-chunk layout, with BOTH the chunk AND the
// block's own storage allocated from the caller's allocator A. This is the
// hook that lets shared_ptr live inside arenas, pools, and pmr worlds.
template <class T, class A = std::allocator<T>, class... Args>
[[nodiscard]] shared_ptr<T> allocate_shared(const A& a, Args&&... args) {
    using Block = detail::CountedInPlaceAlloc<T, A>;
    using BlockAlloc =
        typename std::allocator_traits<A>::template rebind_alloc<Block>;
    BlockAlloc ba(a);                 // same policy, rebound to the block type
    Block* blk = ba.allocate(1);
    try {
        ::new (static_cast<void*>(blk)) Block(a, std::forward<Args>(args)...);
    } catch (...) {
        ba.deallocate(blk, 1);        // T's ctor threw: return the RAW chunk to A
        throw;
    }
    shared_ptr<T> sp(blk->value(), blk);
    sp.board_self();
    return sp;
}

}  // namespace my

// =================================================== the suite

struct Widget {
    std::string name;
    long payload;
    static inline std::int64_t born = 0, dead = 0;
    Widget(std::string n, long v) : name(std::move(n)), payload(v) { ++born; }
    ~Widget() { ++dead; }
};

// three deleter flavors — three UNRELATED types
struct TraceDel {                    // stateful: 16 bytes of captures
    std::ostringstream* log;
    int id;
    void operator()(Widget* p) const {
        if (log) (*log) << "    [del#" << id << " destroyed '" << p->name << "']\n";
        delete p;
    }
};
struct EmptyDel {                    // stateless: zero bytes
    void operator()(Widget* p) const noexcept { delete p; }
};
struct HugeDel {                     // deliberately fat: 64 bytes of state
    char pad[64];
    void operator()(Widget* p) const { delete p; }
};

// a counting allocator: proof that the block came from OUR arena.
// GOTCHA this file actually hit: template statics are PER-INSTANTIATION, and
// allocate_shared rebinds (ArenaAlloc<Widget> -> ArenaAlloc<CountedInPlaceAlloc<...>>),
// so per-type counters would count in two different variables. The counters
// live in ONE non-template holder every instantiation shares.
struct ArenaStats { static inline int allocs = 0, frees = 0; };
template <class T>
struct ArenaAlloc {
    using value_type = T;
    ArenaAlloc() = default;
    template <class U> ArenaAlloc(const ArenaAlloc<U>&) noexcept {}
    T* allocate(std::size_t n) {
        ++ArenaStats::allocs;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    void deallocate(T* p, std::size_t) noexcept {
        ++ArenaStats::frees;
        ::operator delete(p);
    }
    template <class U> bool operator==(const ArenaAlloc<U>&) const noexcept { return true; }
    template <class U> bool operator!=(const ArenaAlloc<U>&) const noexcept { return false; }
};

int main() {

    SECTION("get_deleter: recovering what was erased");
    {
        std::ostringstream log;

        auto sp1 = my::shared_ptr<Widget>(new Widget("sp1", 1), TraceDel{&log,1});

        TraceDel* td = sp1.get_deleter<TraceDel>();

        CHECK(td != nullptr && td->id == 1, "successfully recovered TraceDel and read id");

        //Mutate before release
        if (td) td->id = 99;
        sp1.reset();
        std::cout << log.str(); // Print the log to see it
        CHECK(log.str().find("del#99") != std::string::npos,
              "disposal log reflects the mutated id (proof we touched the real stored deleter, not a copy)");

        // (c) Ask for the wrong type
        auto sp2 = my::shared_ptr<Widget>(new Widget("sp2", 2), TraceDel{&log, 2});
        EmptyDel* wrong = sp2.get_deleter<EmptyDel>();
        CHECK(wrong == nullptr, "asking for the wrong type returns nullptr");
        // (d) get_deleter on a make_shared handle
        auto sp3 = my::make_shared<Widget>("sp3", 3);
        TraceDel* empty_from_make = sp3.get_deleter<TraceDel>();
        CHECK(empty_from_make == nullptr, "make_shared block returns nullptr (no deleter stored)");
    }
    SECTION_END();
}
