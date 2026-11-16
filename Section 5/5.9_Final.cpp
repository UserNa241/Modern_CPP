// lesson_5_9_capstone.cpp — Section 5 capstone: concept-constrained container + algorithm
//
// ONE idea: combine everything from Section 5 into one piece of code — concepts,
// variadics, folds, type traits, and compile-time computation — so a generic
// container and algorithm are correct BY CONSTRUCTION, not by convention.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_5_9_capstone.cpp -o cap && ./cap

#include <cstddef>
#include <memory>
#include <concepts>
#include <iostream>
#include <type_traits>
#include <vector>

// ============================================================
// CONCEPTS (from lessons 5.4 / 5.5)
// ============================================================

// MyCopyable: T can be copied and assigned — a named, reusable requirement.
template<class T>
concept MyCopyable = std::copyable<T>;

// MyDefaultConstructible: T can be default-initialized.
template<class T>
concept MyDefaultConstructible = std::default_initializable<T>;

// MyAccumulable: init + arg yields T — the requirement for our accumulate fold.
template<class T, class Arg>
concept MyAccumulable = std::is_copy_constructible_v<T> &&
                        requires(T a, Arg b) {
	                        { a + b } -> std::same_as<T>;
                        };

static_assert(MyCopyable<int>);
static_assert(MyCopyable<std::string>);
static_assert(!MyCopyable<std::unique_ptr<int> >);

static_assert(MyDefaultConstructible<int>);
static_assert(MyDefaultConstructible<std::string>);

static_assert(MyAccumulable<int, int>);
static_assert(MyAccumulable<std::string, std::string>);

// ============================================================
// CONCEPT-CONSTRAINED CONTAINER: fixed-capacity stack vector
// ============================================================

// T must be default-constructible (we pre-allocate N slots) and copyable
// (we copy values in). Both are enforced by concepts at the class head.
template<MyDefaultConstructible T, std::size_t N>
	requires MyCopyable<T>
class StaticVector {
	T data_[N] = {}; // default-construct all N slots (requires MyDefaultConstructible)
	std::size_t size_ = 0;

public:
	constexpr StaticVector() = default;

	// Variadic constructor: every argument must be convertible to T.
	// (Fold expression from lesson 5.3; type trait from lesson 5.6.)
	template<class... Args>
		requires (std::convertible_to<Args, T> && ...) && (sizeof...(Args) <= N)
	constexpr StaticVector(Args &&... args) : data_{std::forward<Args>(args)...}, size_(sizeof...(Args)) {}

	constexpr void push_back(const T &v) {
		if (size_ < N) { // runtime guard; capacity is fixed
			data_[size_] = v; // copy assignment (requires MyCopyable)
			++size_;
		}
	}

	constexpr T &operator[](std::size_t i) {
		return data_[i];
	}

	constexpr const T &operator[](std::size_t i) const {
		return data_[i];
	}

	constexpr std::size_t size() const {
		return size_;
	}

	constexpr std::size_t capacity() const {
		return N;
	}

	constexpr bool empty() const {
		return size_ == 0;
	}

	constexpr T *begin() {
		return data_;
	}

	constexpr T *end() {
		return data_ + size_;
	}

	constexpr const T *begin() const {
		return data_;
	}

	constexpr const T *end() const {
		return data_ + size_;
	}
};

// Deduction guide is NOT written on purpose — class templates need explicit
// arguments (lesson 5.1). StaticVector<int, 5> is spelled out.

static_assert(MyCopyable<int>);
using Vec5 = StaticVector<int, 5>; // OK: int satisfies both concepts

// ============================================================
// CONCEPT-CONSTRAINED ALGORITHM: accumulate with a fold (lesson 5.3)
// ============================================================

// init and every arg must be accumulable into T (lesson 5.6 trait + concept).
template<class T, class... Args>
	requires (MyAccumulable<T, Args> && ...) // C++20: every Args satisfies MyAccumulable<T, Arg>
constexpr T accumulate(T init, Args... args) {
	return (init + ... + args); // binary left fold (lesson 5.3)
}

// ============================================================
// Compile-time verification (lesson 5.7)
// ============================================================

constexpr Vec5 make_vec() {
	Vec5 v{1, 2, 3, 4, 5}; // variadic constructor (fold + convertible_to)
	return v; // copy (MyCopyable)
}

constexpr int total = accumulate(0, 1, 2, 3, 4, 5); // fold: (((0+1)+2)+3)+4+5 = 15
static_assert(total == 15);

constexpr Vec5 v = make_vec();
static_assert(v.size() == 5);
static_assert(v[0] == 1);
static_assert(v[4] == 5);

int main() {
	std::cout << "=== Concept-constrained StaticVector ===\n";
	Vec5 v1{10, 20, 30};
	std::cout << "  size=" << v1.size() << " capacity=" << v1.capacity() << "\n";
	std::cout << "  elements: ";
	for (int x: v1) std::cout << x << " ";
	std::cout << "\n";

	v1.push_back(40);
	std::cout << "  after push_back(40): size=" << v1.size() << " last=" << v1[3] << "\n";

	std::cout << "\n=== Concept-constrained accumulate (fold) ===\n";
	std::cout << "  accumulate(0, 1,2,3,4,5) = " << accumulate(0, 1, 2, 3, 4, 5) << "\n";
	std::cout << "  compile-time total          = " << total << "\n";

	std::cout << "\n=== Compile-time verified (static_assert) ===\n";
	std::cout << "  v[0]=" << v[0] << " v[4]=" << v[4] << " size=" << v.size() << "\n";

	return 0;
}
