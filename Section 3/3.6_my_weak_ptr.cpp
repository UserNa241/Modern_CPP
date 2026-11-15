// lesson_3_6_my_weak_ptr.cpp — Phase B, part 2: my::weak_ptr + the cycle test.
//
// The library : everything from 3.5, plus:
//               ControlBlock::try_add_strong() — the CAS engine under lock()
//               my::weak_ptr<T>                — the observer handle
//               shared_ptr aliasing ctor       — share lifetime, not identity
// The proof   : the roadmap's cycle test — a my::shared_ptr cycle leaks with
//               its counts frozen LIVE (ledger gap + LSan certificate), the
//               weak_ptr back-edge version unwinds clean; the hostage window
//               printed open; lock() as check-and-hold.
//
//               Paired Constraint B, part 2: weak-count management lives
//               inside the library. User code boards observers and asks
//               questions; it never touches a counter.
//
// Build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_3_6_my_weak_ptr.cpp -o wk && ./wk
// MSVC : cl /std:c++20 /EHsc /W4 lesson_3_6_my_weak_ptr.cpp

#include <iostream>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <type_traits>

namespace my {

	// Forward declarations (these were probably already there)
	template <class T> class shared_ptr;
	template <class T> class weak_ptr;

	// NEW — forward declaration for the class we're about to build
	template <class T> class enable_shared_from_this;

	namespace detail {
		inline bool g_quiet = false;
		inline std::int64_t g_blocks_born = 0, g_blocks_gone = 0;

		class ControlBlock {
		public:
			ControlBlock() noexcept {
				++g_blocks_born;
				if (!g_quiet) {
					std::cout << "	[block " << static_cast<void *>(this) << "	born	|	strong = 1 weak = 1(seat)]\n";
				}
			}

			void add_strong() noexcept {
				auto old = strong_.fetch_add(1, std::memory_order_relaxed);
				if (!g_quiet) std::cout << "	[+strong" << old << "->" << old + 1 << "]\n";
			}

			void add_weak() noexcept {
				auto old = weak_.fetch_add(1, std::memory_order_relaxed);
				if (!g_quiet) std::cout << "	[+weak" << old << "->" << old + 1 << "]\n";
			}

			bool try_add_strong() noexcept {
				auto n = strong_.load(std::memory_order_relaxed);
				while (n != 0) {
					if (strong_.compare_exchange_weak(n, n + 1,
					                                  std::memory_order_acq_rel, std::memory_order_relaxed)) {
						if (!g_quiet) std::cout << "	[+strong" << n << "->" << n + 1 << "]\n";
						return true;
					}
				}
				return false;
			}

			void release_strong() noexcept {
				auto old = strong_.fetch_sub(1, std::memory_order_acq_rel);
				if (!g_quiet) std::cout << "	[-strong" << old << "->" << old - 1 << "]\n";
				if (old == 1) {
					dispose();
					release_weak();
				}
			}

			void release_weak() noexcept {
				auto old = weak_.fetch_sub(1, std::memory_order_acq_rel);
				if (!g_quiet) std::cout << "	[-weak" << old << "->" << old - 1 << "]\n";
				if (old == 1) {
					destroy();
				}
			}

			std::int32_t use_count() const noexcept {
				return strong_.load(std::memory_order_relaxed);
			}

		protected:
			void gone() noexcept {
				++g_blocks_gone;
				if (!g_quiet) {
					std::cout << "	[block " << static_cast<void *>(this) << " freed]\n";
				}
			}

			~ControlBlock() = default;

		private:
			virtual void dispose() noexcept = 0;
			virtual void destroy() noexcept = 0;

			std::atomic<std::int32_t> strong_{1};
			std::atomic<std::int32_t> weak_{1};
		};

		// layout 1: block and T in separate chunks
		template<class T>
		class CountedSeparate final : public ControlBlock {
		public:
			explicit CountedSeparate(T *p) : p_(p) {}

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

		// layout 2: [block header | T] in ONE chunk (make_shared)
		template<class T>
		class CountedInPlace final : public ControlBlock {
		public:
			template<class... A>
			explicit CountedInPlace(A &&... a) {
				try {
					::new(static_cast<void *>(buf_)) T(std::forward<A>(a)...);
				} catch (...) {
					gone();
					throw;
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
	}

	// OWNER HANDLE SHARED POINTER
	template<class T>
	class shared_ptr {
	public:
		shared_ptr() noexcept : p_(nullptr), blk_(nullptr) {}
		explicit shared_ptr(T *p) : p_(p), blk_(new detail::CountedSeparate<T>(p)) {
			if constexpr (std::is_base_of_v<enable_shared_from_this<T>,T>) {
				p->weak_this_ = *this;
			}
		}

		shared_ptr(const shared_ptr &o) noexcept : p_(o.p_), blk_(o.blk_) {
			if (blk_) blk_->add_strong();
		}

		shared_ptr(shared_ptr &&o) noexcept : p_(o.p_), blk_(o.blk_) {
			o.p_ = nullptr;
			o.blk_ = nullptr;
		}

		~shared_ptr() {
			if (blk_) {
				blk_->release_strong();
			}
		}

		shared_ptr &operator=(const shared_ptr &o) {
			shared_ptr tmp(o);
			swap(tmp);
			return *this;
		}

		shared_ptr &operator=(shared_ptr &&o) noexcept {
			shared_ptr tmp(std::move(o));
			swap(tmp);
			return *this;
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

		const void *block_address() const noexcept {
			return blk_;
		}

		template<class U>
		shared_ptr(const shared_ptr<U> &owner, T *p) noexcept : p_(p), blk_(owner.blk_) {
			if (blk_) {
				blk_->add_strong();
			}
		}

	private:
		shared_ptr(T *p, detail::ControlBlock *b) noexcept : p_(p), blk_(b) {
			if constexpr (std::is_base_of_v<enable_shared_from_this<T>, T>) {
				p->weak_this_ = *this;
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

	// OBSERVER HANDLE WEAK POINTER
	template<class T>
	class weak_ptr {
	public:
		weak_ptr() noexcept : p_(nullptr), blk_(nullptr) {}

		weak_ptr(const shared_ptr<T> &s) noexcept : p_(s.p_), blk_(s.blk_) {
			if (blk_) {
				blk_->add_weak();
			}
		}

		weak_ptr(const weak_ptr& o) : p_(o.p_), blk_(o.blk_) {
			if (blk_) {
				blk_->add_weak();
			}
		}

		weak_ptr(weak_ptr &&o) noexcept : p_(o.p_), blk_(o.blk_) {
			o.p_ = nullptr;
			o.blk_ = nullptr;
		}

		~weak_ptr() {
			if (blk_) {
				blk_->release_weak();
			}
		}

		weak_ptr& operator=(const weak_ptr &o) {
			weak_ptr tmp(o);
			swap(tmp);
			return *this;
		}

		weak_ptr& operator=(weak_ptr&& o) noexcept {
			weak_ptr tmp(std::move(o));
			swap(tmp);
			return *this;
		}

		weak_ptr &operator=(const shared_ptr<T> &s) {
			weak_ptr tmp(s);
			swap(tmp);
			return *this;
		}

		void swap(weak_ptr &o) noexcept {
			std::swap(p_, o.p_);
			std::swap(blk_, o.blk_);
		}

		// IS THE OBJECT GONE?
		bool expired() const noexcept {
			return !blk_ || blk_->use_count() == 0;
		}

		// USE COUNT
		std::int32_t use_count() const noexcept {
			return (blk_ ? blk_->use_count() : 0);
		}

		shared_ptr<T> lock() const noexcept {
			if (blk_ && blk_->try_add_strong()) {
				return shared_ptr<T>(p_, blk_);
			}
			return shared_ptr<T>();
		}

		void reset() noexcept {
			weak_ptr tmp;
			swap(tmp);
		}

		const void *block_address() const noexcept {
			return blk_;
		}

	private:
		T *p_;
		detail::ControlBlock *blk_;
	};

	template<class T, class... A>
	[[nodiscard]] shared_ptr<T> make_shared(A &&... a) {
		auto *blk = new detail::CountedInPlace<T>(std::forward<A>(a)...);
		return shared_ptr<T>(blk->value(), blk);
	}

	template <class T>
	class enable_shared_from_this {
	protected:
		enable_shared_from_this() = default;
		enable_shared_from_this(const enable_shared_from_this&) = default;
		enable_shared_from_this& operator=(const enable_shared_from_this&) {
			return *this;
		}
		~enable_shared_from_this() = default;

	public:
		shared_ptr<T> shared_from_this() {
			return weak_this_.lock();
		}

		weak_ptr<T> weak_from_this() noexcept {
			return weak_this_;
		}

	private:
		// WHY weak_ptr and not shared_ptr:
		// If this member were shared_ptr<T>, then every object owned by a
		// shared_ptr would already hold a strong reference to itself. Combined
		// with the external owner's reference, strong count would be 2 at birth.
		// When the external owner lets go, the count drops to 1 — held by the
		// object's own member. That count can never reach 0, so ~T() never runs,
		// the Control Block never frees, and the object leaks forever:
		// a length-1 self-cycle. weak_ptr avoids this by observing without owning.
		weak_ptr<T> weak_this_;

		template <class U> friend class shared_ptr;
 	};
}

struct Widget {
	std::string name;
	long payload;

	Widget(std::string n, long v) : name(std::move(n)), payload(v) {
		std::cout << "	+ Widget '" << name << "'\n";
	}

	~Widget() {
		std::cout << "	-Widget '" << name << "'\n";
	}
};

struct BadNode {
	std::string name;
	my::shared_ptr<BadNode> next, prev;

	explicit BadNode(std::string n) : name(std::move(n)) {
		std::cout << "	+Bad '" << name << "'\n";
	}

	~BadNode() {
		std::cout << "	-Bad" << name << "'\n";
	}
};

struct GoodNode {
	std::string name;
	my::shared_ptr<GoodNode> next;
	my::weak_ptr<GoodNode> prev;

	explicit GoodNode(std::string n) : name(std::move(n)) {
		std::cout << "	+Good '" << name << "'\n";
	}

	~GoodNode() {
		std::cout << "	-Good'" << name << "'\n";
	}
};


struct S : my::enable_shared_from_this<S> {
	std::string name;
	explicit S(std::string n) : name(std::move(n)) {
		std::cout << "  + S '" << name << "'\n";
	}
	~S() {
		std::cout << "  - S '" << name << "'\n";
	}

	my::shared_ptr<S> self() {
		return shared_from_this();
	}
};

int main() {
	// Note: must use the public raw-adoption constructor, NOT make_shared!
	auto sp = my::shared_ptr<S>(new S("demo"));
	std::cout << "use_count before self(): " << sp.use_count() << "\n";   // 1

	auto got = sp->self();                                                // shared_from_this()
	std::cout << "use_count after self():  " << sp.use_count() << "\n";   // 2
	std::cout << "got->name: " << got->name << "\n";                      // "demo"

	// Both handles share the same Control Block.
	// When both go out of scope, count → 0, block frees.
}