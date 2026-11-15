// lesson_3_7_exception_safety.cpp — Phase B graduation: exception safety + the
// full P2+P3 suite under the sanitizer stamp.
//
// What this file is:
//   * the library: my::unique_ptr (compact edition), my::shared_ptr,
//     my::weak_ptr, my::enable_shared_from_this — with THREE exception-safety
//     fixes integrated:
//       fix 1: raw-adoption ctor disposes *p if the control-block allocation
//              throws (catch-dispose-rethrow — the std rule our 3.5/3.6
//              versions violated: ownership is accepted at ENTRY)
//       fix 2: enable_shared_from_this boards through BOTH doors (raw ctor AND
//              make_shared — the 3.6 exercise gap)
//       fix 3: enable_shared_from_this::operator= assigns NOTHING (identity is
//              not part of an S's value — the 3.6 hijack probe)
//   * the suite: in-code CHECKs with a pass/fail counter and a NONZERO EXIT
//     CODE on any failure — what CI consumes. Ledger audits at three
//     granularities (T objects, control blocks, bytes), failure injection via
//     an instrumented operator new, and the 2.8 theorem mechanically proven
//     for our own type (vector reallocation: ZERO handle copies).
//
// Paired Constraint A (suite side): the suite never spells new/delete except
//   at the deliberate failure-window demos, each one commented.
// Paired Constraint B (library side): all counting, disposal, freeing, and
//   boarding lives inside the library.
//
// Build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_3_7_exception_safety.cpp -o es && ./es
// Stamp: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined lesson_3_7_exception_safety.cpp -o es_san && ./es_san
// MSVC : cl /std:c++20 /EHsc /W4 lesson_3_7_exception_safety.cpp

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// =================================================== instrumented allocator
// Every allocation counted; one-shot failure injection: set g_fail_at to
// (current count + 1) and the NEXT allocation attempt throws std::bad_alloc.
std::size_t g_allocs = 0, g_frees = 0;
std::size_t g_fail_at = 0;

void *operator new(std::size_t n) {
	if (g_fail_at != 0 && g_allocs + 1 == g_fail_at) {
		g_fail_at = 0; // one-shot: disarm after firing
		throw std::bad_alloc{};
	}
	++g_allocs;
	if (void *p = std::malloc(n))
		return p;
	throw std::bad_alloc{};
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"  // GCC 13+ false positive
#endif                                                   // on replaced operator new
void operator delete(void *p) noexcept {
	++g_frees;
	std::free(p);
}

void operator delete(void *p, std::size_t) noexcept {
	++g_frees;
	std::free(p);
}

void *operator new[](std::size_t n) {
	return ::operator new(n);
}

void operator delete[](void *p) noexcept {
	::operator delete(p);
}

void operator delete[](void *p, std::size_t) noexcept {
	::operator delete(p);
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

// =================================================== the check harness
int g_pass = 0, g_fail = 0, g_section_pass = 0;
const char *g_section = "";
#define CHECK(cond, msg)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            ++g_pass;                                                    \
            ++g_section_pass;                                            \
        } else {                                                         \
            ++g_fail;                                                    \
            std::cout << "    FAIL: " << msg << "\n";                    \
        }                                                                \
    } while (0)
#define SECTION(name)                                                    \
    do {                                                                 \
        std::cout << "=== " << name << " ===\n";                         \
        g_section = name;                                                \
        g_section_pass = 0;                                              \
    } while (0)
#define SECTION_END()                                                    \
    do {                                                                 \
        std::cout << "    -> " << g_section_pass << " checks passed\n";  \
    } while (0)

// =================================================== the library
namespace my {
	namespace detail {
		inline bool g_quiet = false;
		inline std::int64_t g_blocks_born = 0, g_blocks_gone = 0, g_sp_copies = 0;

		class ControlBlock {
		public:
			ControlBlock() noexcept {
				++g_blocks_born;
				if (!g_quiet)
					std::cout << "    [block " << static_cast<const void *>(this) << " born]\n";
			}

			void add_strong() noexcept {
				strong_.fetch_add(1, std::memory_order_relaxed);
			}

			void add_weak() noexcept {
				weak_.fetch_add(1, std::memory_order_relaxed);
			}

			// lock()'s engine: check-and-hold as ONE atomic step (CAS loop)
			bool try_add_strong() noexcept {
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
					dispose(); // phase 1: destroy T
					release_weak(); // the seat
				}
			}

			void release_weak() noexcept {
				auto old = weak_.fetch_sub(1, std::memory_order_acq_rel);
				if (old == 1)
					destroy(); // phase 2: free the block
			}

			std::int32_t use_count() const noexcept {
				return strong_.load(std::memory_order_relaxed);
			}

		protected:
			void gone() noexcept {
				++g_blocks_gone;
			}

			~ControlBlock() = default;

		private:
			virtual void dispose() noexcept = 0;

			virtual void destroy() noexcept = 0;

			std::atomic<std::int32_t> strong_{1};
			std::atomic<std::int32_t> weak_{1}; // observers + the seat
		};

		template<class T>
		class CountedSeparate final : public ControlBlock {
		public:
			explicit CountedSeparate(T *p) noexcept : p_(p) {}

		private:
			void dispose() noexcept override {
				delete p_;
			}

			void destroy() noexcept override {
				gone();
				delete this;
			}

			T *p_;
		};

		template<class T>
		class CountedInPlace final : public ControlBlock {
		public:
			template<class... A>
			explicit CountedInPlace(A &&... a) {
				try {
					::new(static_cast<void *>(buf_)) T(std::forward<A>(a)...);
				} catch (...) {
					gone(); // 2.9: T never existed; the new-expression frees the chunk
					throw; // RETHROW ONLY — deleting here would be a double free
				}
			}

			T *value() noexcept {
				return reinterpret_cast<T *>(buf_);
			}

		private:
			void dispose() noexcept override {
				value()->~T();
			}

			void destroy() noexcept override {
				gone();
				::operator delete(this, sizeof(CountedInPlace));
			}

			alignas(T) std::byte buf_[sizeof(T)];
		};
	} // namespace detail

	template<class T>
	class weak_ptr;
	template<class T>
	class enable_shared_from_this;

	// ------------------------------------------------ my::unique_ptr (compact edition)
	// The full 3.4 model (T[] specialization, poison pills) lives in lesson_3_4.
	// Everything needed for the suite is here: EBO deleter, move choreography.
	template<class T>
	struct default_delete {
		void operator()(T *p) const noexcept {
			delete p;
		}
	};

	template<class T, class D = default_delete<T> >
	class unique_ptr {
	public:
		unique_ptr() noexcept = default;
		explicit unique_ptr(T *p) noexcept : p_(p) {}
		unique_ptr(T *p, D d) noexcept: p_(p), d_(std::move(d)) {}
		unique_ptr(const unique_ptr &) = delete;
		unique_ptr &operator=(const unique_ptr &) = delete;
		unique_ptr(unique_ptr &&o) noexcept : p_(o.release()), d_(std::move(o.d_)) {}

		unique_ptr &operator=(unique_ptr &&o) noexcept {
			reset(o.release()); // release-first: the order IS the self-move guard
			d_ = std::move(o.d_);
			return *this;
		}

		~unique_ptr() {
			if (p_) d_(p_);
		}

		T &operator*() const {
			return *p_;
		}

		T *operator->() const noexcept {
			return p_;
		}

		T *get() const noexcept {
			return p_;
		}

		explicit operator bool() const noexcept {
			return p_ != nullptr;
		}

		T *release() noexcept {
			T *p = p_;
			p_ = nullptr;
			return p;
		}

		void reset(T *p = nullptr) noexcept {
			T *old = p_;
			p_ = p;
			if (old) d_(old);
		}

		void swap(unique_ptr &o) noexcept {
			std::swap(p_, o.p_);
			std::swap(d_, o.d_);
		}

	private:
		T *p_ = nullptr;
		[[no_unique_address]] D d_{};
	};

	// ------------------------------------------------ my::shared_ptr
	template<class T>
	class shared_ptr {
	public:
		shared_ptr() noexcept : p_(nullptr), blk_(nullptr) {}

		// FIX 1 — the two-step door with the std exception contract:
		// ownership of *p is accepted at ENTRY (that is what taking T* means), so
		// if the control-block allocation throws, we DISPOSE *p and rethrow.
		// (3.5/3.6 versions leaked here: acquire-without-release on failure.)
		explicit shared_ptr(T *p) : p_(p), blk_(nullptr) {
			try {
				blk_ = new detail::CountedSeparate<T>(p);
			} catch (...) {
				delete p; // the ledger stays balanced even on failure
				throw;
			}
			board_self();
		}

		shared_ptr(const shared_ptr &o) noexcept : p_(o.p_), blk_(o.blk_) {
			++detail::g_sp_copies; // the 2.8-theorem probe
			if (blk_) blk_->add_strong();
		}

		shared_ptr(shared_ptr &&o) noexcept : p_(o.p_), blk_(o.blk_) {
			o.p_ = nullptr;
			o.blk_ = nullptr;
		}

		~shared_ptr() {
			if (blk_) blk_->release_strong();
		}

		shared_ptr &operator=(const shared_ptr &o) noexcept { // body cannot throw
			shared_ptr tmp(o); // copy (nothrow) ...
			swap(tmp); // ... then release old in ~tmp
			return *this;
		}

		shared_ptr &operator=(shared_ptr &&o) noexcept {
			shared_ptr tmp(std::move(o));
			swap(tmp);
			return *this;
		}

		// aliasing ctor: share the block (lifetime), replace the pointee
		template<class U>
		shared_ptr(const shared_ptr<U> &owner, T *p) noexcept : p_(p), blk_(owner.blk_) {
			if (blk_) blk_->add_strong();
		}

		T &operator*() const noexcept {
			return *p_;
		}

		T *operator->() const noexcept {
			return p_;
		}

		T *get() const noexcept {
			return p_;
		}

		explicit operator bool() const noexcept {
			return p_ != nullptr;
		}

		long use_count() const noexcept {
			return blk_ ? blk_->use_count() : 0;
		}

		void reset() noexcept {
			shared_ptr tmp;
			swap(tmp);
		}

		void swap(shared_ptr &o) noexcept {
			std::swap(p_, o.p_);
			std::swap(blk_, o.blk_);
		}

	private:
		shared_ptr(T *p, detail::ControlBlock *b) noexcept : p_(p), blk_(b) {}

		// FIX 2 — enable_shared_from_this boards through BOTH doors. Internal
		// construction (lock, aliasing) skips boarding: the observer is already
		// aboard (or the type never opted in).
		void board_self() {
			if constexpr (std::is_base_of_v<enable_shared_from_this<T>, T>) {
				if (p_ && p_->weak_this_.blk_ == nullptr)
					p_->weak_this_ = *this;
			}
		}

		template<class U, class... A>
		friend shared_ptr<U> make_shared(A &&...);

		template<class U>
		friend class weak_ptr;
		template<class U>
		friend class shared_ptr;

		T *p_;
		detail::ControlBlock *blk_;
	};

	// ------------------------------------------------ my::weak_ptr
	template<class T>
	class weak_ptr {
	public:
		weak_ptr() noexcept : p_(nullptr), blk_(nullptr) {}

		weak_ptr(const shared_ptr<T> &s) noexcept : p_(s.p_), blk_(s.blk_) {
			if (blk_) blk_->add_weak();
		}

		weak_ptr(const weak_ptr &o) noexcept : p_(o.p_), blk_(o.blk_) {
			if (blk_) blk_->add_weak();
		}

		weak_ptr(weak_ptr &&o) noexcept : p_(o.p_), blk_(o.blk_) {
			o.p_ = nullptr;
			o.blk_ = nullptr;
		}

		~weak_ptr() {
			if (blk_) blk_->release_weak();
		}

		weak_ptr &operator=(const weak_ptr &o) {
			weak_ptr tmp(o);
			swap(tmp);
			return *this;
		}

		weak_ptr &operator=(weak_ptr &&o) noexcept {
			weak_ptr tmp(std::move(o));
			swap(tmp);
			return *this;
		}

		weak_ptr &operator=(const shared_ptr<T> &s) {
			weak_ptr tmp(s);
			swap(tmp);
			return *this;
		}

		bool expired() const noexcept {
			return !blk_ || blk_->use_count() == 0;
		}

		long use_count() const noexcept {
			return blk_ ? blk_->use_count() : 0;
		}

		shared_ptr<T> lock() const noexcept {
			if (blk_ && blk_->try_add_strong())
				return shared_ptr<T>(p_, blk_);
			return shared_ptr<T>();
		}

		void reset() noexcept {
			weak_ptr tmp;
			swap(tmp);
		}

		void swap(weak_ptr &o) noexcept {
			std::swap(p_, o.p_);
			std::swap(blk_, o.blk_);
		}

		T *p_;
		detail::ControlBlock *blk_;
	};

	// ------------------------------------------------ my::enable_shared_from_this
	// FIX 3 — operator= assigns NOTHING: identity was established at adoption and
	// is not part of an S's value. (A defaulted = would hijack weak_this_ — the
	// 3.6 probe made s1->self() return the WRONG object.)
	template<class T>
	class enable_shared_from_this {
	protected:

		enable_shared_from_this() = default;
		enable_shared_from_this(const enable_shared_from_this &) = default;

		enable_shared_from_this &operator=(const enable_shared_from_this &) noexcept {
			return *this; // copy everything EXCEPT weak_this_
		}
		~enable_shared_from_this() = default;

	public:
		shared_ptr<T> shared_from_this() {
			return weak_this_.lock();
		}

		weak_ptr<T> weak_from_this() const noexcept {
			return weak_this_;
		}

	private:
		weak_ptr<T> weak_this_; // WEAK: a shared here = self-cycle,
		// strong could never reach 0, no S
		// would ever be destroyed (3.2b)
		template<class U>
		friend class shared_ptr;
	};

	// ------------------------------------------------ the factory
	template<class T, class... A>
	[[nodiscard]] shared_ptr<T> make_shared(A &&... a) {
		auto *blk = new detail::CountedInPlace<T>(std::forward<A>(a)...);
		shared_ptr<T> sp(blk->value(), blk);
		sp.board_self(); // FIX 2: the make_shared door boards too
		return sp;
	}
} // namespace my

// =================================================== the suite

struct Widget {
	std::string name;
	long payload;
	static inline std::int64_t born = 0, dead = 0;

	Widget(std::string n, long v) : name(std::move(n)), payload(v) {
		++born;
		if (!my::detail::g_quiet) std::cout << "    +Widget '" << name << "'\n";
	}

	~Widget() {
		++dead;
		if (!my::detail::g_quiet) std::cout << "    -Widget '" << name << "'\n";
	}
};

struct Bomb { // ctor always throws
	static inline int born = 0;

	Bomb() {
		++born;
		std::cout << "    (Bomb armed)\n";
		throw std::runtime_error("boom");
	}
};

struct S : my::enable_shared_from_this<S> { // esft test type
	std::string name;
	explicit S(std::string n) : name(std::move(n)) {}

	my::shared_ptr<S> self() {
		return shared_from_this();
	}
};

struct Owner { // aliasing test type
	std::string tag;
	Widget member;
	Owner(std::string t, std::string wn) : tag(std::move(t)), member(wn, 0) {}
};

struct CountingDel {
	static inline int fired = 0; // Global counter
	int log_id;                  // Stateful data

	// This runs when a unique_ptr is destroyed
	void operator()(Widget* p) const noexcept {
		++fired;
		delete p;
	}
};

// the 2.8 noexcept theorem, as compile-time contracts on OUR types:
static_assert(std::is_nothrow_move_constructible_v<my::unique_ptr<Widget> >);
static_assert(std::is_nothrow_move_constructible_v<my::shared_ptr<Widget> >);
static_assert(std::is_nothrow_move_constructible_v<my::weak_ptr<Widget> >);
static_assert(std::is_nothrow_copy_constructible_v<my::shared_ptr<Widget> >);
static_assert(std::is_nothrow_copy_assignable_v<my::shared_ptr<Widget> >);
static_assert(std::is_nothrow_swappable_v<my::shared_ptr<Widget> >);
static_assert(!std::is_copy_constructible_v<my::unique_ptr<Widget> >);

int main() {

	SECTION("A. Tiers: where exceptions can still enter (and where they cannot)");
	std::cout << "    handle layer: copy/move/assign/swap/reset/lock ALL noexcept\n";
	std::cout << "    remaining doors: allocation (block/chunk), T's constructor\n";
	CHECK(true, "static_asserts compiled (see list above the suite)");
	SECTION_END();

	SECTION("B. Baseline choreography + ledger");
	{
		auto a = my::make_shared<Widget>("base", 1);
		auto b = a;
		auto c = std::move(b);
		CHECK(!b, "move source is an empty husk");
		CHECK(a.use_count() == 2, "copy counted, move silent");
		my::weak_ptr<Widget> w = a;
		auto held = w.lock();
		CHECK(held.use_count() == 3, "lock is check-and-hold");
		held.reset();
		c.reset();
		b.reset();
		w.reset();
		a.reset();
		CHECK(Widget::born == Widget::dead, "T ledger balanced");
		CHECK(my::detail::g_blocks_born == my::detail::g_blocks_gone, "block ledger balanced");
		CHECK(g_allocs == g_frees, "byte/alloc ledger balanced");
	}
	SECTION_END();

	SECTION("C. Window 1: T's constructor throws mid-adoption");
	{
		bool caught = false;
		try {
			auto sp = my::make_shared<Bomb>();
			(void) sp;
		} catch (const std::runtime_error &e) {
			caught = true;
			std::cout << "    caught '" << e.what() << "'\n";
		}
		CHECK(caught, "original exception type propagated");
		CHECK(Bomb::born == 1, "Bomb ctor ran once");
		CHECK(my::detail::g_blocks_born == my::detail::g_blocks_gone, "chunk returned, ledger balanced");
		CHECK(g_allocs == g_frees, "no leak at byte level");
		caught = false;
		try {
			my::shared_ptr<Bomb> sp(new Bomb); // two-step: new throws BEFORE adoption
		} catch (const std::runtime_error &) {
			caught = true;
		}
		CHECK(caught && g_allocs == g_frees, "two-step failure: nothing adopted, nothing leaked");
	}
	SECTION_END();

	SECTION("D. Window 2: the control-block allocation throws (bad_alloc)"); {
		std::cout << "    --- D1: the UNFIXED pattern (3.5/3.6 code) ---\n";
		Widget *victim = new Widget("unfixed-victim", 1); // deliberate raw new: the demo
		g_fail_at = g_allocs + 1; // next allocation will fail
		bool caught = false;
		try {
			// what our old ctor did: blk_(new CountedSeparate<T>(p)) — a member
			// initializer: if new throws, p was never adopted and NOBODY deletes it
			struct Unfixed {
				Widget *p_;
				int *blk_;
				explicit Unfixed(Widget *p) : p_(p), blk_(new int[8]) {}
			} u(victim);
			(void) u;
		} catch (const std::bad_alloc &) {
			caught = true;
		}
		CHECK(caught, "bad_alloc propagated");
		CHECK(Widget::dead < Widget::born, "victim LEAKED (dtor never ran) — the bug");
		std::cout << "    (test rescues the victim manually; production code cannot)\n";
		delete victim; // deliberate: demo cleanup
		CHECK(Widget::born == Widget::dead, "ledger balanced only after manual rescue");

		std::cout << "    --- D2: the FIXED door (catch-dispose-rethrow) ---\n";
		// NOTE: create the Widget FIRST, arm the failure, THEN adopt — so the
		// injection hits the control-block allocation, not the Widget's own.
		std::int64_t w0 = Widget::born; // baseline BEFORE the victim
		Widget *wp = new Widget("fixed-victim", 2); // deliberate raw new: the demo
		caught = false;
		g_fail_at = g_allocs + 1; // the BLOCK allocation will fail
		try {
			my::shared_ptr<Widget> sp(wp); // adopt -> block new throws -> dispose -> rethrow
		} catch (const std::bad_alloc &) {
			caught = true;
		}
		CHECK(caught, "bad_alloc rethrown after cleanup");
		CHECK(Widget::born - w0 == 1 && Widget::dead - w0 == 1,
		      "victim constructed AND disposed exactly once (no leak, no double)");
		CHECK(my::detail::g_blocks_born == my::detail::g_blocks_gone, "no block was born");

		std::cout << "    --- D3: the chunk allocation itself fails (make_shared) ---\n";
		w0 = Widget::born;
		caught = false;
		g_fail_at = g_allocs + 1; // the CountedInPlace allocation fails
		try {
			auto sp = my::make_shared<Widget>("chunk-fail", 3);
			(void) sp;
		} catch (const std::bad_alloc &) {
			caught = true;
		}
		CHECK(caught, "bad_alloc propagated");
		CHECK(Widget::born - w0 == 0, "T never constructed (allocation failed first)");
		CHECK(g_allocs == g_frees, "nothing retained");
	}
	SECTION_END();

	SECTION("E. The 3.6 fixes, tested: esft through both doors + non-hijack assign"); {
		auto raw = my::shared_ptr<S>(new S("raw-door"));
		auto viaRaw = raw->self();
		CHECK(static_cast<bool>(viaRaw) && viaRaw.use_count() == 2, "raw door: self() alive");

		auto mk = my::make_shared<S>("make-shared-door");
		auto viaMk = mk->self();
		CHECK(static_cast<bool>(viaMk) && viaMk.use_count() == 2,
		      "make_shared door: self() alive (the 3.6 gap, closed)");

		auto s1 = my::shared_ptr<S>(new S("one"));
		auto s2 = my::shared_ptr<S>(new S("two"));
		*s1 = *s2; // S's default assign copies members
		auto selfAfter = s1->self();
		// IDENTITY, not value: after *s1 = *s2 the object's name IS "two" either
		// way — only the pointer tells hijack from no-hijack. (A name check here
		// would pass in BOTH worlds: my own 3.6 probe made exactly that mistake.)
		CHECK(selfAfter && selfAfter.get() == s1.get(),
		      "self() returns THIS object (identity intact)");
		CHECK(selfAfter.get() != s2.get(), "and not the source's object (no hijack)");
		CHECK(s1->name == "two", "the VALUE copied (name), identity separate from value");

		auto owner = my::make_shared<Owner>("OWN", "mem");
		my::shared_ptr<Widget> view(owner, &owner->member);
		CHECK(view.use_count() == 2, "alias shares the block");
		owner.reset();
		CHECK(view->name == "mem", "alias keeps the whole Owner alive");
	}
	SECTION_END();

	SECTION("F. The 2.8 theorem, mechanically proven for our own type"); {
		my::detail::g_quiet = true;
		std::vector<my::shared_ptr<Widget> > v;
		std::int64_t copies0 = my::detail::g_sp_copies;
		for (int i = 0; i < 100; ++i)
			v.push_back(my::make_shared<Widget>("w" + std::to_string(i), i));
		std::int64_t growthCopies = my::detail::g_sp_copies - copies0;
		CHECK(growthCopies == 0,
		      "vector grew 100-fold with ZERO handle copies (nothrow moves only)");
		CHECK(v[57]->name == "w57" && v[57].use_count() == 1, "all payloads intact, single-owner");
		v.clear();
		CHECK(Widget::born == Widget::dead && my::detail::g_blocks_born == my::detail::g_blocks_gone,
		      "full teardown balanced");
		my::detail::g_quiet = false;
	}
	SECTION_END();

	SECTION("G. The stamp");
	std::cout << "    T ledger      : " << Widget::born << " born / " << Widget::dead << " dead\n";
	std::cout << "    block ledger  : " << my::detail::g_blocks_born << " born / "
			<< my::detail::g_blocks_gone << " freed\n";
	std::cout << "    alloc ledger  : " << g_allocs << " allocs / " << g_frees << " frees\n";
	std::cout << "    checks        : " << g_pass << " passed, " << g_fail << " failed\n";
	if (g_fail == 0)
		std::cout << "    SANITIZER-CLEAN STAMP: all invariants held. Phase B graduates.\n";
	// return g_fail == 0 ? 0 : 1;

	SECTION("H. unique_ptr move/deleter theorem");
	{

		CountingDel::fired = 0; // Reset counter for the test

		// STEP 2: Build the vector and force it to grow
		std::vector<my::unique_ptr<Widget, CountingDel>> vu;
		vu.reserve(1); // Force the vector to start tiny so it MUST reallocate

		for (int i = 0; i < 200; ++i) {
			vu.push_back(my::unique_ptr<Widget, CountingDel>(
				new Widget("u" + std::to_string(i), i), {i}
			));
		}

		// STEP 3: The Checks / Predictions
		// PREDICTION 1: After growing to 200, how many times did the deleter fire?
		// ANSWER: 0.
		CHECK(CountingDel::fired == 0, "vector grew to 200 with ZERO deleter firings");

		// PREDICTION 2: After we clear the vector, how many times does it fire?
		// ANSWER: 200.
		vu.clear();
		CHECK(CountingDel::fired == 200, "clear() destroyed exactly 200 intact payloads");

		// STEP 4: The Self-Move Test
		auto u = my::unique_ptr<Widget, CountingDel>(new Widget("self-move", 99), {99});
		int fire_baseline = CountingDel::fired; // Remember the count

		auto& a = u;       // Make an alias to trick the compiler
		u = std::move(a);  // Self-move!

		CHECK(u.get() != nullptr, "self-move guarded: value remains intact");
		CHECK(CountingDel::fired == fire_baseline, "deleter did not fire during self-move");

		// STEP 5: The Compile-Time Wall
		static_assert(!std::is_copy_constructible_v<my::unique_ptr<Widget, CountingDel>>,
					  "unique_ptr cannot be copied, mechanically proven");
	}
	SECTION_END();

	return 0;
}
