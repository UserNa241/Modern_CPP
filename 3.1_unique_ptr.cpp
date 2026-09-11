// lesson_3_1_unique_ptr.cpp — portable, standard C++ (out_ptr needs C++23)
// build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_3_1_unique_ptr.cpp -o up && ./up
#include <iostream>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <string>

// ---------- deleter #3: stateless lambda for FILE* ----------
struct FileCloser { // a functor: stateless, zero-size
	void operator()(std::FILE *f) const noexcept {
		std::cout << "  [fclose " << (f ? "ok" : "null") << "]\n";
		if (f) std::fclose(f);
	}
};

using CFile = std::unique_ptr<std::FILE, FileCloser>;

// ---------- deleter #4: STATEFUL lambda ----------
struct Trace {
	static inline int deletes = 0;
};

void stateful_demo() {
	int sessionId = 7;
	auto traced = [sessionId](int *p) { // carries state!
		++Trace::deletes;
		std::cout << "  [session " << sessionId << " deletes " << p << "]\n";
		delete p;
	};
	std::unique_ptr<int, decltype(traced)> p{new int(42), traced};
	std::cout << "  value = " << *p << "\n";
} // stateful deleter fires with its captured session id

// ---------- C-API interop: out_ptr ----------
extern "C" int make_temp_file(std::FILE **out); // pretend C library
int make_temp_file(std::FILE **out) {
	*out = std::fopen("outptr_demo.txt", "w");
	return *out ? 0 : -1;
}

// ---------- the zero-overhead pair (for disassembly) ----------
int raw_read(int *p) {
	return *p;
}

int uniq_read(std::unique_ptr<int> p) {
	return *p;
} // BY VALUE: same code expected

int main() {
	std::cout << "=== 1. Sizes: the empty-deleter promise ===\n";
	std::cout << "  sizeof(int*)              = " << sizeof(int *) << "\n"
			<< "  sizeof(unique_ptr<int>)   = " << sizeof(std::unique_ptr<int>) << "\n"
			<< "  sizeof(unique_ptr<int, FileCloser>) = "
			<< sizeof(CFile) << "\n"
			<< "  sizeof(stateful-deleter ptr) = "
			<< sizeof(std::unique_ptr<int, decltype([](int *p) {
				delete p;
			})>) << "\n";

	std::cout << "\n=== 2. The array specialization ===\n";
	auto arr = std::make_unique<int[]>(8);
	for (int i = 0; i < 8; ++i) arr[i] = i * i;
	std::cout << "  arr[7] = " << arr[7] << "\n";
	// arr.get()[3] ok; *arr is a compile error (array owner cannot dereference)

	std::cout << "\n=== 3. Stateful deleter at work ===\n";
	stateful_demo();
	std::cout << "  Trace::deletes = " << Trace::deletes << "\n";

	std::cout << "\n=== 4. Moves, not copies (the 2.8 choreography) ===\n";
	std::unique_ptr<int> a = std::make_unique<int>(10);
	std::unique_ptr<int> b = std::move(a); // steal
	std::cout << "  after move: a=" << (a ? "holds" : "empty")
			<< ", b=" << *b << "\n";
	a = std::make_unique<int>(20); // husk refilled
	std::cout << "  refilled: a=" << *a << " b=" << *b << "\n";

	std::cout << "\n=== 5. out_ptr: C-API acquisition in one step ===\n";
	CFile f;
	if (make_temp_file(std::out_ptr(f)) == 0) { // writes back into f!
		std::fputs("written via out_ptr", f.get());
		std::cout << "  out_ptr acquired; file written\n";
	}

	std::cout << "\n=== 6. Ownership ledger audit ===\n";
	std::cout << "  (destructor prints above/below are the ledger entries)\n";

	std::unique_ptr<int[]> arr0{new int[5]};
	std::cout << *arr0;
	return 0;
}
