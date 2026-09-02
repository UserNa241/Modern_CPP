// lesson_2_3_exception_safety.cpp — portable, standard C++ only
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_2_3_exception_safety.cpp -o es && ./es
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>

// ---------------- the bomb: copy throws when armed ----------------
struct Book {
	std::string title;

	Book() = default;

	explicit Book(std::string t) : title(std::move(t)) {}

	Book(const Book &o) : title(o.title) {
		if (fail_on_copy) throw std::runtime_error("Book copy exploded");
	}

	Book(Book &&o) noexcept : title(std::move(o.title)) {}

	Book &operator=(const Book &) = default; // note: copy-assign uses string's,
	Book &operator=(Book &&) noexcept = default; // not our bomb (bomb is on COPY CTOR)
	static inline bool fail_on_copy = false;
};

// ---------------- naive assignment: basic (at best) ----------------
class NaiveLibrary {
	std::vector<Book> shelf_;

public:
	NaiveLibrary &operator=(const NaiveLibrary &o) {
		shelf_.clear(); // 1. destroy MY state first...
		shelf_.reserve(o.shelf_.size());
		for (const Book &b: o.shelf_) shelf_.push_back(b); // 2. ...then fallible work
		return *this; //    throw mid-loop -> I am HALF-COPIED
	}

	explicit NaiveLibrary(const std::initializer_list<Book> il) : shelf_(il) {}

	std::size_t size() const noexcept {
		return shelf_.size();
	}
};

// ---------------- copy-and-swap: STRONG ----------------
class StrongLibrary {
	std::vector<Book> shelf_;

public:
	StrongLibrary(const StrongLibrary &) = default; // re-enabled: declaring the move-op
	// below had DELETED it (Lesson 2.2 table!)
	StrongLibrary &operator=(const StrongLibrary &o) {
		StrongLibrary tmp(o); // ALL fallible work on the side
		swap(tmp); // commit: no-throw pointer trade
		return *this; // tmp destroys my OLD shelf
	}

	StrongLibrary &operator=(StrongLibrary &&o) noexcept { // move-assign: rob expiring
		shelf_ = std::move(o.shelf_);
		return *this;
	}

	void swap(StrongLibrary &o) noexcept {
		shelf_.swap(o.shelf_); // vector swap: 3 pointer trades, no throw
	}

	explicit StrongLibrary(std::initializer_list<Book> il) : shelf_(il) {}

	std::size_t size() const noexcept {
		return shelf_.size();
	}
};

int main() {
	std::cout << "=== 1. Naive assignment under failure: BASIC exposed ===\n";
	{
		NaiveLibrary full{Book{"A"}, Book{"B"}, Book{"C"}, Book{"D"}, Book{"E"}};
		NaiveLibrary victim{Book{"X"}};
		std::cout << "  victim before: " << victim.size() << " book\n";
		Book::fail_on_copy = true; // arm the bomb (copies of Book throw)
		try {
			victim = full;
		} // 1 + 2 copies succeed... 3rd throws?
		catch (const std::exception &e) {
			std::cout << "  threw: " << e.what() << "\n";
		}
		Book::fail_on_copy = false;
		std::cout << "  victim after : " << victim.size()
				<< " books  <- HALF-COPIED: cleared, then partially refilled\n"
				<< "     (valid? yes. Unchanged? NO. That is only the basic guarantee)\n";
	}

	std::cout << "\n=== 2. Copy-and-swap under the SAME failure: STRONG ===\n";
	{
		StrongLibrary full{Book{"A"}, Book{"B"}, Book{"C"}, Book{"D"}, Book{"E"}};
		StrongLibrary victim{Book{"X"}};
		std::cout << "  victim before: " << victim.size() << " book\n";
		Book::fail_on_copy = true;
		try {
			victim = full;
		} catch (const std::exception &e) {
			std::cout << "  threw: " << e.what() << "\n";
		}
		Book::fail_on_copy = false;
		std::cout << "  victim after : " << victim.size()
				<< " book  <- BIT-FOR-BIT THE ORIGINAL: commit-or-rollback held\n";
	}

	std::cout << "\n=== 3. The happy path pays the same price ===\n";
	Book::fail_on_copy = false;
	StrongLibrary a{Book{"A"}, Book{"B"}};
	StrongLibrary b{Book{"Z"}};
	a = b; // copy into tmp, swap, destroy old
	std::cout << "  after a = b: a.size=" << a.size() << " (and strong, for free)\n";
}
