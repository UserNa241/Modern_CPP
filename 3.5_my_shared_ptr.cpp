// lesson_3_5_my_shared_ptr.cpp — Phase B, part 1: the control block made real.
//
// The library : my::detail::ControlBlock (strong/weak atomics + two virtuals),
//               CountedSeparate<T> (two-step layout), CountedInPlace<T>
//               (make_shared layout), my::shared_ptr<T>, my::make_shared.
// The proof   : every count change PRINTS from inside the library — you watch
//               copies climb, moves stay silent, and the two-phase death
//               (dispose the object, then free the block) line by line.
//               Plus: chunk-adjacency for make_shared vs the two-step form,
//               a 2-thread atomic hammer (no lost updates), a throwing ctor
//               (chunk returned), and a born-vs-freed block ledger.
//
//               Paired Constraint B, part 1: ALL counting, disposal and freeing
//               happens inside the library. User code never touches a count.
//
// Build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread lesson_3_5_my_shared_ptr.cpp -o sp && ./sp
// MSVC : cl /std:c++20 /EHsc /W4 lesson_3_5_my_shared_ptr.cpp

#include <iostream>
#include <utility>
#include <type_traits>
#include <memory>
#include <thread>
#include <sstream>
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace my {
	namespace detail {
		inline bool g_quiet = false;
		inline std::int64_t g_blocks_born = 0, g_blocks_gone = 0;

		// ------------------------------------------------- the control block
		// Layout on x86-64: [vptr 8][strong 4][weak 4] = 16 bytes of header.
		//
		// strong_ = number of shared_ptr owners.
		// weak_   = number of weak_ptr observers PLUS ONE: the strong owners hold a
		//           collective "seat" taken at birth and released when strong hits 0.
		//           The seat is not decoration — it is a correctness mechanism (theory
		//           1.4): it guarantees "who frees the block" is decided by ONE atomic
		//           counter, never by two counters agreeing.

		class ControlBlock {
		public:
			ControlBlock() noexcept {
				++g_blocks_born;
				if (!g_quiet) {
					std::cout << "    [block " << static_cast<const void *>(this)
							<< " born | strong=1 weak=1(seat)]\n";
				}
			}

			void add_strong() noexcept {
				auto old = strong.fetch_add(1, std::memory_order_relaxed);
				if (!g_quiet) std::cout << "	[+strong " << old << "->" << old + 1 << "]\n";
			}

			void add_weak() noexcept {
				weak.fetch_add(1, std::memory_order_relaxed);
			}

			void release_strong() noexcept {
				auto old = strong.fetch_sub(1, std::memory_order_acq_rel);

				if (old == 1) {
					dispose();
					release_weak();
				}
			}

			void release_weak() noexcept {
				auto old = weak.fetch_sub(1, std::memory_order_acq_rel);

				if (old == 1) {
					destroy();
				}
			}

			std::int32_t use_count() const noexcept {
				return strong.load(std::memory_order_relaxed);
			}

		protected:
			void gone() noexcept {
				++g_blocks_gone;
				if (!g_quiet) {
					std::cout << "[block " << static_cast<const void *>(this) << "freed]\n";
				}
			}

			~ControlBlock() = default;

		private:
			// dispose: called once, when the LAST shared_ptr dies. Destroy T. FREE NOTHING.
			// destroy: called once, when the block itself dies (weak, incl. seat, hits 0).
			// Two SEPARATE virtuals because the two layouts free different things:
			//   in-place  : T lives inside the block -> dispose runs ~T() only;
			//               destroy frees the whole chunk.
			//   separate  : T lives elsewhere        -> dispose deletes T;
			//               destroy frees only the block.
			virtual void dispose() noexcept = 0;
			virtual void destroy() noexcept = 0;

			std::atomic<std::int32_t> strong{1};
			std::atomic<std::int32_t> weak{1};
		};

		template <class T>
		struct default_delete {
			void operator () (T* p) const noexcept {
				delete p;
			}
		};

		//Layout 1 : Block and T in separate hunk chunks
		template<class T, class D = default_delete<T> >
		class CountedSeparate final : public ControlBlock {
		public:
			explicit CountedSeparate(T *p, D d = D{}) noexcept : p_(p), d_(std::move(d)) {}

		private:
			void dispose() noexcept override {
					d_(p_);
			}

			void destroy() noexcept override {
				gone();
				delete this;
			}
			T* p_;
			[[no_unique_address]]D d_;
		};

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

	template<class T>
	class shared_ptr {

	public:
		shared_ptr() noexcept : p_(nullptr), blk_(nullptr) {}
		explicit shared_ptr(T *p) : p_(p), blk_(new detail::CountedSeparate<T>(p_)) {}

		template<class D>
		shared_ptr(T*p, D d) : p_(p), blk_(new detail::CountedSeparate<T, D>(p, std::move(d))) {}

		shared_ptr(const shared_ptr &o) noexcept : p_(o.p_), blk_(o.blk_) {
			if (blk_) {
				blk_->add_strong();
			}
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

		void reset() noexcept {
			shared_ptr temp;
			swap(temp);
		}

		shared_ptr& operator =(const shared_ptr &o) {
			shared_ptr tmp(o);
			swap(tmp);
			return *this;
		}

		shared_ptr& operator=(shared_ptr &&o) noexcept {
			shared_ptr tmp(std::move(o));
			swap(tmp);
			return *this;
		}

		void swap(shared_ptr &o) noexcept {
			std::swap(p_, o.p_);
			std::swap(blk_, o.blk_);
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

		const void *block_address() const {
			return blk_;
		}

		long use_count () const noexcept {
			return blk_? blk_->use_count() : 0;
		}

	private:
		T *p_;
		detail::ControlBlock *blk_;

		shared_ptr(T *p, detail::ControlBlock* b) noexcept : p_(p), blk_(b) {}

		template<class U, class... A>
		friend shared_ptr<U> make_shared(A &&...);
	};

	template<class U, class... A>
	shared_ptr<U> make_shared(A &&... a) {
		auto *blk = new detail::CountedInPlace<U>(std::forward<A>(a)...);
		return shared_ptr<U>(blk->value(), static_cast<detail::ControlBlock*>(blk));
	}
}

struct Widget {
	std::string name;
	long payload;

	Widget(std::string n, long v) : name(std::move(n)), payload(v) {
		if (!my::detail::g_quiet) {
			std::cout << "	+Widget '" << name << "'\n";
		}
	}

	~Widget() {
		if (!my::detail::g_quiet) {
			std::cout << "	-Widget '" << name << "'\n";
		}
	}
};

struct FatDeleter {
	int id;
	std::ostringstream* log;
	void operator () (Widget* p) const {
		(*log) << "	[del#" << id << "	deletes Widget '" << p->name << "']\n";
		delete p;
	}
};

// This must compile! If the handle grew, this assert fails.
static_assert(sizeof(my::shared_ptr<Widget>) == 16);

// Even with a fat deleter:
// (Note: shared_ptr's template is only over T, not D, so this is the same type)
static_assert(sizeof(my::shared_ptr<Widget>) == sizeof(std::shared_ptr<Widget>));

int main() {

	std::cout << "  sizeof(Widget)  = " << sizeof(Widget) << "\n";
	std::cout << "  sizeof(my::detail::ControlBlock) = " << sizeof(my::detail::ControlBlock) << " (vptr + two 4-byte counts)\n";
	std::cout << "	sizeof(my::detail::CountedInPlace<Widget>) = " << sizeof(my::detail::CountedInPlace<Widget>) << "\n";
	std::cout << "	sizeof(my::detail::CountedSeparate<Widget>) = " << sizeof(my::detail::CountedSeparate<Widget>) << "\n";
	std::cout << "  sizeof(my::shared_ptr<Widget>)  = " << sizeof(my::shared_ptr<Widget>) << "\n";

	{
		auto a = my::make_shared<Widget>("alpha", 1);
		std::cout << "  use_count=" << a.use_count() << "\n";

		auto b = a; // [+strong 1->2]
		auto c = a; // [+strong 2->3]
		std::cout << "  after two copies: use_count=" << a.use_count() << "\n";
	}

	my::detail::g_quiet = true;
	std::cout << "Thread Hammering Test\n";
	{
		auto master = my::make_shared<Widget>("Hammered", 0);
		constexpr int itr = 1000;

		auto hammer = [&] () {
			for (int i = 0; i < itr; ++i) {
				auto tmp = master;
			}
		};
		std::jthread t1(hammer), t2(hammer);
		t1.join();
		t2.join();
		std::cout << "  final use_count = " << master.use_count() << "\n";

		master.reset();
	}

	std::cout << "Checking Deleter Function" << "\n";
	std::ostringstream log;
	{
		auto a = my::shared_ptr<Widget>(new Widget("Widget1",42), FatDeleter{1, &log});
		auto b = my::shared_ptr<Widget>(new Widget("Widget2",42), FatDeleter{2, &log});
	}
	std::cout << log.str();

	auto p = my::shared_ptr<Widget>(new Widget("hello", 42));

	return 0;
}
