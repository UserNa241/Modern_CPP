// lesson_3_9_intrusive_atomics.cpp — Phase C: intrusive counting, atomic_ref,
// and atomic<shared_ptr> for real.
//
// The design under test: the count lives INSIDE the object (COM-style
// AddRef/Release), not in a separate control block. One pointer per handle,
// one allocation per object, one cache line for count+payload.
//
//   A. the intrusive base + handle, lifecycle instrumented
//   B. sizes: 8-byte handle, one-line object — vs the block world's 16/2
//   C. THE BENCHMARKS (the 3.3 prediction, measured):
//      C1 copy-churn     : unique-move base vs intrusive vs make_shared vs two-step
//      C2 creation-churn : full create+destroy cycles, allocation counts included
//      C3 contention     : two threads on one count — do the designs converge?
//   D. interop: std::shared_ptr wrapping an intrusive object (and the trap)
//   E. std::atomic_ref: atomicity as a VIEW (the InterlockedIncrement story)
//   F. std::atomic<std::shared_ptr<T>>: the two-word atomic, for real
//
// Build: g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -pthread lesson_3_9_intrusive_atomics.cpp -o ia && ./ia
// (opt)  g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -mcx16 lesson_3_9_intrusive_atomics.cpp -o ia16 && ./ia16
//        (-mcx16 enables cmpxchg16b; spoiler: libstdc++'s answer STILL doesn't
//         change — F explains the deeper reason why)
// MSVC : cl /std:c++20 /EHsc /W4 /O2 lesson_3_9_intrusive_atomics.cpp

#include <iostream>
#include <atomic>
#include <chrono>
#include <memory>
#include <new>
#include <utility>
#include <cstdlib>
#include <cstdint>
#include <vector>

// / =================================================== instrumented allocator
// THREAD-SAFE on purpose: destruction is multithreaded in this lesson (F's
// reader thread destroys shared_ptrs while main allocates). This file's own
// first build had these as plain size_t — two threads ++g_frees concurrently,
// a lost update made the ledger CHECK fail intermittently. The instrument
// raced; the measured code was fine. Sanitize the ruler too.
std::atomic<std::size_t> g_allocs{0}, g_frees{0};

void *operator new(std::size_t n) {
	g_allocs.fetch_add(1, std::memory_order_relaxed);
	if (void *p = std::malloc(n)) {
		return p;
	}
	throw std::bad_alloc{};
}

void operator delete(void *p) noexcept {
	g_frees.fetch_add(1, std::memory_order_relaxed);
	std::free(p);
}

void operator delete(void *p, std::size_t) noexcept {
	g_frees.fetch_add(1, std::memory_order_relaxed);
	std::free(p);
}

void *operator new[](std::size_t n) {
	return ::operator new(n);
}

void operator delete[](void *p) noexcept {
	g_frees.fetch_add(1, std::memory_order_relaxed);
	std::free(p);
}

void operator delete[](void *p, std::size_t) noexcept {
	g_frees.fetch_add(1, std::memory_order_relaxed);
	std::free(p);
}

// =================================================== check harness
int g_pass = 0, g_fail = 0, g_section_pass = 0;
const char* g_section = "";
bool g_talk = true;                       // per-event prints (off during benches)
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

using Clock = std::chrono::steady_clock;

namespace my {

	namespace detail {
		inline bool g_quiet = false;
		inline std::int64_t g_blocks_born = 0, g_blocks_gone = 0;

		class ControlBlock {
		public:
			ControlBlock() noexcept {++g_blocks_born;}
			void add_strong() noexcept {
				strong_.fetch_add(1, std::memory_order_relaxed);
			}
			void release_strong() noexcept {
				auto old = strong_.fetch_sub(1,std::memory_order_acq_rel);
				if (old == 1) {
					dispose();
					release_weak();
				}
			}

			void release_weak() noexcept {
				if (weak_.fetch_sub(1,std::memory_order_acq_rel) == 1) {
					destroy();
				}
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
			std::atomic<std::int32_t>weak_{1};
		};

		template <class T>
		class CountedSeparate final: public ControlBlock {
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
			T* p_;
		};

		template <class T>
		class CountedInPlace final: public ControlBlock {
		public:
			template <class... A>
			explicit CountedInPlace(A&& ...a) {
				try {
					::new (static_cast<void*>(buf_)) T(std::forward<A>(a)...);
				} catch (...) {
					gone();
					throw;
				}
			}
			T* value() noexcept {
				return reinterpret_cast<T*> (buf_);
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

	template <class T>
	class shared_ptr {
	public:
		shared_ptr() noexcept: p_(nullptr), blk_(nullptr) {}
		explicit shared_ptr(T* p) : p_(p), blk_(new detail::CountedSeparate<T>(p)) {}
		~shared_ptr() {
			if (blk_) {
				blk_->release_strong();
			}
		}
		shared_ptr(const shared_ptr& o) noexcept : p_(o.p_), blk_(o.blk_) {
			if (blk_) blk_->add_strong();
		}
		shared_ptr(shared_ptr&& o) noexcept : p_(o.p_), blk_(o.blk_) {
			o.p_ = nullptr;
			o.blk_ = nullptr;
		}
		shared_ptr& operator= (const shared_ptr& o)	noexcept {
			shared_ptr tmp(o);
			swap(tmp);
			return *this;
		}
		shared_ptr& operator= (shared_ptr&& o) noexcept {
			shared_ptr tmp(std::move(o));
			swap(tmp);
			return *this;
		}
		void swap(shared_ptr& o) noexcept {
			std::swap(p_,o.p_);
			std::swap(blk_,o.blk_);
		}

		T& operator*() noexcept {
			return *p_;
		}
		T* operator->() noexcept {
			return p_;
		}
		T* get() const noexcept {
			return p_;
		}
		explicit operator bool() const noexcept {
			return blk_ != nullptr;
		}
		long use_count() const noexcept {
			return blk_? blk_->use_count() : 0;
		}
		void reset() noexcept {
			shared_ptr tmp;
			swap(tmp);
		}
	private:
		shared_ptr(T* p, detail::ControlBlock* b) noexcept : p_(p), blk_(b) {}
		T* p_;
		detail::ControlBlock* blk_;

		template <class U, class... A>
		friend shared_ptr<U> make_shared(A&&...);
	};

	template<class U, class... A>
	shared_ptr<U> make_shared(A&&... a) {
		auto* blk = new detail::CountedInPlace<U>(std::forward<A>(a)...);
		return shared_ptr<U> (blk->value(),blk);
	}

	//Intrusive World
	// The count lives INSIDE the object, as its first member. COM's IUnknown is
	// this exact shape: AddRef / Release / (QueryInterface).
	struct RefCounted {
	public:
		void retain() const noexcept {
			auto old = refs_.fetch_add(1, std::memory_order_relaxed);
			if (g_talk) std::cout << "	[refs " << old << "->" << old + 1 << "]\n";
		}

		void release() const noexcept {
			auto old = refs_.fetch_sub(1, std::memory_order_acq_rel);
			if (g_talk) std::cout << "	[refs " << old << "->" << old - 1 << "]\n";
			if (old == 1) {
				delete this;
			}
		}
		long refs() const noexcept {
			return refs_.load(std::memory_order_relaxed);
		}
	protected:
		RefCounted() = default;
		// Copying an object does NOT copy its count: every object is born with 1
		// reference to itself (the esft lesson one level down).
		RefCounted(const RefCounted&) noexcept {}
		RefCounted& operator=(const RefCounted&) noexcept { return *this; }
		virtual ~RefCounted() = default;
		//virtual: release deletes via base ptr

	private:
		mutable std::atomic<long> refs_{1};
	};

	//ONE pointer, No block pointer - there is no block.
	template <class T>
	class intrusive_ptr {
	public:
		intrusive_ptr() noexcept = default;
		// ADOPTS a reference without retain() — the one-adoption rule (the 3.2
		// trap's exact shape: two handles built from one raw pointer = double
		// delete, because neither construction knows about the other).
		explicit intrusive_ptr(T* p) noexcept : p_(p) {}
		intrusive_ptr(const intrusive_ptr& o) noexcept : p_(o.p_) {
			if (p_) p_->retain();
		}
		intrusive_ptr(intrusive_ptr&& o) noexcept : p_(o.p_) {
			o.p_ = nullptr;
		}
		~intrusive_ptr() {
			if (p_) {
				p_->release();
			}
		}
		// Unified by-value assignment: an lvalue argument copy-constructs the
		// parameter (retain), an rvalue move-constructs it (silent) — then swap
		// + ~param does the release. One body, both semantics.
		intrusive_ptr& operator=(intrusive_ptr o) noexcept {
			swap(o);
			return *this;
		}
		T& operator*() const noexcept {return *p_;}
		T* operator->() const noexcept {return p_;}
		T* get() const noexcept {return p_;}
		explicit operator bool() const noexcept {return p_ != nullptr;}
		T* release_handle() noexcept {
			T*p = p_;
			p_ = nullptr;
			return p;
		}
		void reset() noexcept {
			intrusive_ptr tmp;
			swap(tmp);
		}
		void swap(intrusive_ptr& o) {std::swap(p_,o.p_);}
	private:
		T* p_ = nullptr;
	};
}

struct INode : my::RefCounted {
	char payload[48];
	static inline long born = 0, dead = 0;
	explicit INode(char c) {
		++born; payload[0] = c;
	}
	~INode() {
		++dead;
		if (g_talk) std::cout << "	-INode\n";
	}
};

struct SNode {
	char payload[48];
	static inline long born = 0, dead = 0;
	explicit SNode(char c) {
		++born; payload[0] = c;
	}
	~SNode() {
		++dead;
	}
};

struct LegacyNode {                        // ABI-frozen "C header" type for atomic_ref
	long refcount;                         // PLAIN long: atomicity is a protocol
	char payload[48];
	static inline long born = 0, dead = 0;
	explicit LegacyNode(char c) : refcount(1) { ++born; payload[0] = c; }
	~LegacyNode() { ++dead; }
};
void retain(LegacyNode* n) noexcept {
	std::atomic_ref<long> rc(n->refcount);
	rc.fetch_add(1, std::memory_order_relaxed);
}
void release(LegacyNode* n) noexcept {
	std::atomic_ref<long> rc(n->refcount);
	if (rc.fetch_sub(1, std::memory_order_acq_rel) == 1)
		delete n;
}

int main() {
	SECTION("A. The intrusive lifecycle, instrumented");
	{
		my::intrusive_ptr<INode> a(new INode('a'));   // born refs=1
		CHECK(a->refs() == 1, "born with one reference");
		auto b = a;                                   // retain: 1->2
		CHECK(a->refs() == 2, "copy AddRefs");
		auto c = std::move(b);                        // silent move
		CHECK(a->refs() == 2 && !b, "move is silent; source husked");
		c.reset();                                    // release: 2->1
		CHECK(a->refs() == 1, "one reference left");
		a.reset();                                    // release: 1->0 -> delete this
		CHECK(INode::born == INode::dead, "object ended itself exactly once");
	}
	SECTION_END();

	SECTION("B. Sizes: what each design costs to HOLD");
	{
		std::cout << "    sizeof(intrusive_ptr<INode>)       = " << sizeof(my::intrusive_ptr<INode>)
				  << "   (one pointer)\n";
		std::cout << "    sizeof(my::shared_ptr<SNode>)      = " << sizeof(my::shared_ptr<SNode>)
				  << "   (two pointers)\n";
		std::cout << "    sizeof(INode)                      = " << sizeof(INode)
				  << "   [vptr|count|payload] still one line, ONE allocation\n";
		std::cout << "    sizeof(CountedInPlace<SNode>)      = " << sizeof(my::detail::CountedInPlace<SNode>)
				  << "   [header|payload] one line, one allocation\n";
		std::cout << "    two-step shared: SNode + CountedSeparate = two allocations, TWO lines\n";
		std::cout << "    ...and NOTHING survives an INode's death: no block, no observer to ask.\n";
		CHECK(sizeof(my::intrusive_ptr<INode>) == 8 && sizeof(my::shared_ptr<SNode>) == 16,
			  "intrusive halves the handle");
	}
	SECTION_END();

	return 0;
}


