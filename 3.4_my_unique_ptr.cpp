#include <iostream>
#include <memory>
#include <utility>
#include <type_traits>
#include <cstddef>

// ========================================================================
// 1. MY UNIQUE_PTR IMPLEMENTATION
// ========================================================================
namespace my {
	// --- 1A. Default Deleters ---
	template<class T>
	struct default_delete
	{
		default_delete() noexcept = default;

		template <class U>
		default_delete(const default_delete<U> &) noexcept {
			static_assert(std::is_convertible_v<U*, T*>);
		}

		void operator()(T *p) const noexcept {
			delete p;
		}
	};

	template<class T>
	struct default_delete<T[]> {
		void operator()(T *p) const noexcept {
			delete[] p;
		}
	};

	// --- 1B. Primary Template (Single Objects) ---
	template<class T, class D = default_delete<T> >
	class unique_ptr {

	public:
		using element_type = T;
		using deleter_type = D;
		using pointer      = T*;

		unique_ptr() noexcept = default;
		unique_ptr(std::nullptr_t) noexcept : p_(nullptr) {}
		explicit unique_ptr(T *p) noexcept : p_(p) {}
		unique_ptr(element_type *p, const deleter_type &d) noexcept : p_(p), d_(d) {}

		template<class U, class E>
		requires (std::is_convertible_v<U*, T*>,
			std::is_constructible_v<D, E&&>)
		unique_ptr(unique_ptr<U, E>&& other) noexcept : p_(other.release()), d_((std::move(other.get_deleter()))) {
			static_assert(!std::is_array_v<U>);
		}

		~unique_ptr() {
			if (p_) d_(p_);
		}

		// Delete copies
		unique_ptr(const unique_ptr &) = delete;
		unique_ptr &operator=(const unique_ptr &) = delete;

		// Move choreography
		unique_ptr(unique_ptr &&other) noexcept : p_(other.release()), d_(std::move(other.d_)) {}

		unique_ptr &operator=(unique_ptr &&other) noexcept {
			reset(other.release());
			d_ = std::move(other.d_);
			return *this;
		}

		element_type *release() noexcept {
			T *old = p_;
			p_ = nullptr;
			return old;
		}

		void reset(T *p = nullptr) noexcept {
			T *old = p_;
			p_ = p;
			if (old) d_(old);
		}

		// Accessors (NO operator[] here!)
		element_type *get() const noexcept {
			return p_;
		}

		element_type &operator*() const {
			return *p_;
		}

		element_type *operator->() const {
			return p_;
		}

		explicit operator bool() const noexcept {
			return p_ != nullptr;
		}

		deleter_type& get_deleter() noexcept {
			return d_;
		}

	private:
		element_type *p_ = nullptr;
		[[no_unique_address]] deleter_type d_{};
	};

	// --- 1C. Array Partial Specialization (T[]) ---
	template<class T, class D>
	class unique_ptr<T[], D> {
	private:
		T *p_ = nullptr;
		[[no_unique_address]] D d_{};

	public:
		unique_ptr() noexcept = default;

		unique_ptr(std::nullptr_t) noexcept : p_(nullptr) {}

		// Poison pill: T must match exactly to avoid array slicing
		template<class U>
		explicit unique_ptr(U *p) noexcept : p_(p) {
			static_assert(std::is_same_v<U, T>, "Array element type must match exactly!");
		}

		unique_ptr(T *p, const D &d) noexcept : p_(p), d_(d) {}

		~unique_ptr() {
			if (p_) d_(p_);
		}

		// Delete copies
		unique_ptr(const unique_ptr &) = delete;

		unique_ptr &operator=(const unique_ptr &) = delete;

		// Move choreography
		unique_ptr(unique_ptr &&other) noexcept : p_(other.release()), d_(std::move(other.d_)) {}

		unique_ptr &operator=(unique_ptr &&other) noexcept {
			reset(other.release());
			d_ = std::move(other.d_);
			return *this;
		}

		T *release() noexcept {
			T *old = p_;
			p_ = nullptr;
			return old;
		}

		void reset(T *p = nullptr) noexcept {
			T *old = p_;
			p_ = p;
			if (old) d_(old);
		}

		// Accessors (HAS operator[], NO operator* or operator->)
		T *get() const noexcept {
			return p_;
		}

		T &operator[](std::size_t i) const {
			return p_[i];
		}

		explicit operator bool() const noexcept {
			return p_ != nullptr;
		}
	};

	// 1. make_unique for Single Objects
	// std::enable_if_t ensures this ONLY runs if T is NOT an array.
	template <class T, class... Args>
	std::enable_if_t<!std::is_array_v<T>, unique_ptr<T>>
	make_unique(Args&&... args) {
		// Forwards the arguments perfectly to the constructor of T
		return unique_ptr<T>(new T(std::forward<Args>(args)...));
	}

	// 2. make_unique for Arrays (T[])
	// std::enable_if_t ensures this ONLY runs if T IS an array.
	template <class T>
	std::enable_if_t<std::is_array_v<T>, unique_ptr<T>>
	make_unique(std::size_t n) {
		// remove_extent_t turns "Widget[]" into "Widget" so new can read it
		using ElementType = std::remove_extent_t<T>;

		// The trailing () initializes the array with zeros/defaults
		return unique_ptr<T>(new ElementType[n]());
	}

} // end namespace my

// ========================================================================
// 2. HELPER TOOLS (To watch memory live and die)
// ========================================================================
struct Widget {
	int id;

	Widget(int i = 0) : id(i) {
		std::cout << "    + Widget " << id << " born\n";
	}

	~Widget() {
		std::cout << "    - Widget " << id << " died\n";
	}
};

struct SimpleTraceDel {
	int trace_id;

	void operator()(Widget *p) const {
		std::cout << "    [Deleter #" << trace_id << "] cleaning up Widget " << p->id << "\n";
		delete p;
	}
};


struct Base {
	virtual ~Base() = default;
};

struct Derived : Base {

};


// ========================================================================
// 3. THE TEST RUNNER
// ========================================================================
int main() {
	std::cout << "========================================\n";
	std::cout << " TEST RUN 1: my::unique_ptr\n";
	std::cout << "========================================\n";

	std::cout << "\nTest A: Single Object & Move Semantics\n"; {
		my::unique_ptr<Widget> ptr1 = my::make_unique<Widget>(10);
		my::unique_ptr<Widget> ptr2 = std::move(ptr1);

		std::cout << "  ptr1 is empty? " << (ptr1.get() == nullptr ? "Yes" : "No") << "\n";
		std::cout << "  ptr2 holds Widget " << ptr2->id << "\n";
	}

	std::cout << "\nTest B: Array Specialization (T[])\n"; {
		// Allocate array of 3 widgets
		my::unique_ptr<Widget[]> arr = my::make_unique<Widget[]>(3);

		arr[0].id = 0;
		arr[1].id = 1;
		arr[2].id = 2;
		std::cout << "  arr[1] holds Widget " << arr[1].id << "\n";
	}

	std::cout << "\nTest C: Stateful Custom Deleter\n";
	{
		my::unique_ptr<Widget, SimpleTraceDel> custom_ptr(new Widget(99), SimpleTraceDel{777});
		std::cout << "  custom_ptr is doing work...\n";
	}

	std::cout << "\nTest D: Derived struct conversion (Upcasting)";
	{
		my::unique_ptr<Base> b = my::unique_ptr<Derived>();
		std::cout << "Conversion worked\n";
	}

	std::cout << "\n\n========================================\n";
	std::cout << " TEST RUN 2: std::unique_ptr (The Baseline)\n";
	std::cout << "========================================\n";

	std::cout << "\nTest A: Single Object & Move Semantics\n"; {
		std::unique_ptr<Widget> ptr1 = std::make_unique<Widget>(10);
		std::unique_ptr<Widget> ptr2 = std::move(ptr1);

		std::cout << "  ptr1 is empty? " << (ptr1.get() == nullptr ? "Yes" : "No") << "\n";
		std::cout << "  ptr2 holds Widget " << ptr2->id << "\n";
	}

	std::cout << "\nTest B: Array Specialization (T[])\n"; {
		std::unique_ptr<Widget[]> arr = std::make_unique<Widget[]>(3);
		std::cout << "  arr[1] holds Widget " << arr[1].id << "\n";
	}

	std::cout << "\nTest C: Stateful Custom Deleter\n"; {
		std::unique_ptr<Widget, SimpleTraceDel> custom_ptr(new Widget(99), SimpleTraceDel{777});
		std::cout << "  custom_ptr is doing work...\n";
	}

	std::cout << "\nTest D: Derived struct conversion (Upcasting)";
	{
		std::unique_ptr<Base> b = std::unique_ptr<Derived>();
		std::cout << "Conversion worked\n";
	}

	std::cout << "\n========================================\n";
	std::cout << " SUCCESS! Outputs should match exactly.\n";
	std::cout << "========================================\n";

	return 0;
}
