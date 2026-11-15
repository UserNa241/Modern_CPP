// lesson_2_8_raii_moves.cpp — portable, standard C++ only
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_2_8_raii_moves.cpp -o mv && ./mv
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// ---------------- instrumented Rule-of-Five buffer ----------------
struct Ledger {
	static inline int allocs = 0, frees = 0, copies = 0, moves = 0;
};

class Buffer {
	int *p_ = nullptr;
	std::size_t n_ = 0;

public:
	explicit Buffer(std::size_t n) : p_(n ? new int[n]{} : nullptr), n_(n) {
		if (n_) {
			++Ledger::allocs;
			std::cout << "  [alloc " << n_ << "]\n";
		}
	}

	~Buffer() {
		if (p_) {
			++Ledger::frees;
			std::cout << "  [free " << n_ << "]\n";
		}
		delete[] p_;
	}

	Buffer(const Buffer &o) : p_(o.n_ ? new int[o.n_]{} : nullptr), n_(o.n_) { // copy
		for (std::size_t i = 0; i < n_; ++i) p_[i] = o.p_[i];
		if (n_) {
			++Ledger::copies;
			std::cout << "  [deep-copy " << n_ << "]\n";
		}
	}

	Buffer &operator=(const Buffer &o) {
		Buffer t(o);
		swap(t);
		return *this;
	} // copy&swap

	Buffer(Buffer &&o) noexcept : p_(o.p_), n_(o.n_) { // MOVE ctor
		o.p_ = nullptr;
		o.n_ = 0; // <- empty the source
		++Ledger::moves;
		std::cout << "  [steal " << n_ << "]\n";
	}

	Buffer &operator=(Buffer &&o) noexcept { // MOVE assign
		if (this != &o) { // <- self-move guard
			delete[] p_; // release MY old resource
			p_ = o.p_;
			n_ = o.n_; // steal
			o.p_ = nullptr;
			o.n_ = 0; // empty source
			++Ledger::moves;
			std::cout << "  [move-assign " << n_ << "]\n";
		} else {
			std::cout << "  [move-assign self: guarded, nothing done]\n";
		}
		return *this;
	}

	void swap(Buffer &o) noexcept {
		std::swap(p_, o.p_);
		std::swap(n_, o.n_);
	}

	bool empty() const noexcept {
		return p_ == nullptr;
	}

	std::size_t size() const noexcept {
		return n_;
	}
};

// ---------------- the noexcept proof: two identical types ----------------
struct MoveOK { // move ctor IS noexcept
	static inline int moves = 0, copies = 0;
	std::string tag;
	explicit MoveOK(std::string t) : tag(std::move(t)) {}

	MoveOK(const MoveOK &o) : tag(o.tag) {
		++copies;
	}

	MoveOK(MoveOK &&o) noexcept : tag(std::move(o.tag)) {
		++moves;
	}

	MoveOK &operator=(const MoveOK &) = default;
	MoveOK &operator=(MoveOK &&) noexcept = default;
};

struct MoveThrows { // identical, but move ctor NOT noexcept
	static inline int moves = 0, copies = 0;
	std::string tag;
	explicit MoveThrows(std::string t) : tag(std::move(t)) {}

	MoveThrows(const MoveThrows &o) : tag(o.tag) {
		++copies;
	}

	MoveThrows(MoveThrows &&o) : tag(std::move(o.tag)) {
		++moves;
	}

	MoveThrows &operator=(const MoveThrows &) = default;
	MoveThrows &operator=(MoveThrows &&) noexcept = default;
};

template<class T>
void grow_report(const char *name) {
	T::moves = 0;
	T::copies = 0;
	std::vector<T> v; // no reserve: forces reallocation
	for (int i = 0; i < 8; ++i) v.emplace_back("x");
	std::cout << "  " << name << ": after 8 push_backs -> "
			<< T::moves << " moves, " << T::copies << " copies\n";
}

int main() {
	std::cout << "=== 1. Choreography: birth, transfer, husk, refill, death ===\n";
	{
		Buffer a(100); // alloc
		Buffer b = std::move(a); // steal: b now owns, a is the husk
		std::cout << "  after steal: a.size=" << a.size()
				<< " (husk), b.size=" << b.size() << " (owner)\n";
		a = Buffer(7); // move-assign: husk refilled (temp steals in)
		std::cout << "  refilled: a.size=" << a.size() << ", b.size=" << b.size() << "\n";
	} // both die: two frees

	std::cout << "  ledger: allocs=" << Ledger::allocs << " frees=" << Ledger::frees
			  << " copies=" << Ledger::copies << " moves=" << Ledger::moves << "\n";

	std::cout << "\n=== 2. THE noexcept THEOREM (vector reallocation) ===\n";
	grow_report<MoveOK>("MoveOK    (move is noexcept)");
	grow_report<MoveThrows>("MoveThrows(move NOT noexcept)");
	std::cout << "  same 8 elements, same growth: one pays MOVES, the other deep COPIES.\n"
			<< "  (capacities 1,2,4,8: transfers 1+2+4 = 7)\n";

	std::cout << "\n=== 3. Self-move: legal expression, guarded survival ===\n";
	Buffer x(5);
	// NOTE: the DIRECT form 'x = std::move(x);' trips GCC's -Wself-move warning
	// (it's statically visible = almost certainly a bug). Real self-moves arrive
	// through ALIASES - a[i] = std::move(a[j]) with i==j - invisible statically:
	Buffer &alias = x; // another name for THE SAME object
	x = std::move(alias); // runtime self-move, compiler can't see it
	std::cout << "  after x = std::move(alias_of_x): x.size=" << x.size()
			<< "  (valid, unchanged - the guard saved us from self-theft)\n";

	std::cout << "\n=== 4. Rule of Zero moves memberwise (implicit, zero code) ===\n";
	struct Holds {
		std::string s;
		Buffer b;
	}; // no special members at all
	Holds h1{"hello", Buffer(3)};
	Holds h2 = std::move(h1); // implicit move: string steals,
	std::cout << "  implicit move: string len=" << h2.s.size() // Buffer steals
			<< ", buffer size=" << h2.b.size() << "\n";
	return 0;
}
