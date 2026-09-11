// lesson_2_9_partial_construction.cpp — portable, standard C++ only
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_2_9_partial_construction.cpp -o pc
//   ./pc          (clean demonstrations)
//   ./pc leak     (broken versions - run under ASan/LSan to see the leaks)
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <chrono>
#include <source_location>
#include <utility>

// ---------- an instrumented member resource ----------
struct Part {
	const char *name;

	explicit Part(const char *n, bool bomb = false) : name(n) {
		std::cout << "    + " << name << "\n";
		if (bomb) throw std::runtime_error(std::string(n) + " exploded");
	}

	~Part() {
		std::cout << "    - " << name << std::endl;
	}

	Part(const Part &) = delete;
	Part &operator=(const Part &) = delete;
};

// ---------- 1. the rule: members unwind, class dtor never runs ----------
struct Machine {
	Part a_, b_, c_;

	Machine() : a_("member A"), b_("member B", /*bomb=*/true), c_("member C") {
		std::cout << "    (Machine ctor body - never reached)\n";
	}

	~Machine() {
		std::cout << "    ~Machine - NEVER PRINTS (no object ever existed)\n";
	}
};

// ---------- 2. raw member (leaks) vs RAII member (clean) ----------
struct RawWorker {
	int *conn_; // RAW: nobody saves it
	Part license_;
	RawWorker() : conn_(new int[256]), license_("license", /*bomb=*/true) {}

	~RawWorker() {
		delete[] conn_;
		std::cout << "    ~RawWorker (never runs on this path)\n";
	}
};

struct FixedWorker {
	std::unique_ptr<int[]> conn_; // RAII: unwinds by itself
	Part license_;

	FixedWorker() : conn_(std::make_unique<int[]>(256)),
	                license_("license", true) {}
};

// ---------- 3. the interleave window, manual vs make_unique ----------
void manual_sequence() { // the pre-C++11 disease
	Part *first = new Part("manual-alloc"); // acquired...
	Part second("manual-second", true); // ...then orphaned by the throw
	delete first; // never reached
}

void make_unique_sequence() {
	auto first = std::make_unique<Part>("owned-alloc"); // owned IMMEDIATELY
	Part second("make_unique-second", true); // throw -> first unwinds
}

// ---------- 4. function-try-block ----------
struct Careful {
	Part inner_;

	Careful() try : inner_("careful-inner", true) {
		std::cout << "    (body never reached)\n";
	} catch (const std::exception &e) {
		std::cout << "    [ftb caught '" << e.what()
				<< "' - member prefix already destroyed; falling off the end RETHOWS]\n";
	}
};

// ---------- 5. scoped timer + source_location, no macros ----------
class ScopedTimer {
	std::source_location loc_;
	std::chrono::steady_clock::time_point t0_;

public:
	explicit ScopedTimer(std::source_location loc = std::source_location::current()) : loc_(loc),
		t0_(std::chrono::steady_clock::now()) { // evaluated AT CALL SITE
		std::cout << "    [enter " << loc_.function_name()
				<< "  (" << loc_.file_name() << ":" << loc_.line() << ")]\n";
	}

	~ScopedTimer() {
		auto us = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - t0_).count();
		std::cout << "    [exit  " << loc_.function_name() << "  after " << us << " us]" << std::endl;
	}

	ScopedTimer(const ScopedTimer &) = delete;
	ScopedTimer &operator=(const ScopedTimer &) = delete;
};

void do_work(bool fail) {
	ScopedTimer t; // captures do_work's location - no macro
	auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(300);
	while (std::chrono::steady_clock::now() < deadline) {} // opaque busy-wait: no volatile
	if (fail) throw std::runtime_error("work failed (exit log still fires)");
}

int main(int argc, char **argv) {
	const bool leak_mode = argc > 1 && std::string(argv[1]) == "leak";

	if (leak_mode) { // broken paths, for LSan to judge
		try {
			RawWorker w;
		} catch (const std::exception &e) {
			std::cout << "  caught " << e.what() << "\n";
		}
		try {
			manual_sequence();
		} catch (const std::exception &e) {
			std::cout << "  caught " << e.what() << "\n";
		}
		return 0;
	}

	std::cout << "=== 1. The rule: ctor throws -> member PREFIX unwinds, dtor never runs ===\n";
	try {
		Machine m;
		(void) m;
	} catch (const std::exception &e) {
		std::cout << "  caught: " << e.what() << "\n";
	}

	std::cout << "\n=== 2. RAII member survives the same failure ===\n";
	try {
		FixedWorker w;
		(void) w;
	} catch (const std::exception &e) {
		std::cout << "  caught: " << e.what() << "\n";
	}

	std::cout << "\n=== 3. make_unique closes the window ===\n";
	try {
		make_unique_sequence();
	} catch (const std::exception &e) {
		std::cout << "  caught: " << e.what() << "\n";
	}

	std::cout << "\n=== 4. function-try-block: log, then implicit rethrow ===\n";
	try {
		Careful c;
		(void) c;
	} catch (const std::exception &e) {
		std::cout << "  caught (rethrown by ftb): " << e.what() << "\n";
	}

	std::cout << "\n=== 5. ScopedTimer + source_location, no macros ===\n";
	do_work(false);
	try {
		do_work(/*fail=*/true);
	} catch (const std::exception &e) {
		std::cout << "  caught: " << e.what() << "\n";
	}
	return 0;
}
