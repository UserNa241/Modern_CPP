// lesson_2_4_handle.cpp — one Handle engine, many resources. Portable standard C++.
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_2_4_handle.cpp -o hd && ./hd
#include <iostream>
#include <cstdio>
#include <cerrno>
#include <mutex>
#include <system_error>
#include <utility>

// ---------- the policies: 3 facts per resource ----------
struct FileTraits {
	using handle_type = std::FILE *;

	static constexpr handle_type invalid() noexcept {
		return nullptr;
	}

	static void release(handle_type h) noexcept {
		std::cout << "    [fclose]\n";
		std::fclose(h);
	}
};

struct HeapTraits {
	using handle_type = int *;

	static constexpr handle_type invalid() noexcept {
		return nullptr;
	}

	static void release(handle_type h) noexcept {
		std::cout << "    [delete[] " << h << "]\n";
		delete[] h;
	}
};

struct MutexTraits {
	using handle_type = std::mutex*;

	static constexpr handle_type invalid() noexcept {
		return nullptr;
	}

	static void release(handle_type h) noexcept {
		std::cout << "	[Release" << h << "]\n";
		h->unlock();
	}
};

// ---------- THE ENGINE (one tested lifetime, parameterized) ----------
template<class Traits>
class Handle {
	typename Traits::handle_type h_ = Traits::invalid();

public:
	Handle() = default;

	explicit Handle(typename Traits::handle_type h) noexcept : h_(h) {}

	~Handle() {
		reset();
	}

	Handle(const Handle &) = delete; // unique ownership; moves return in S4
	Handle &operator=(const Handle &) = delete;

	[[nodiscard]] typename Traits::handle_type get() const noexcept {
		return h_;
	} // BORROW

	[[nodiscard]] typename Traits::handle_type release() noexcept { // HAND OUT
		auto h = h_;
		h_ = Traits::invalid();
		return h;
	}

	void reset(typename Traits::handle_type h = Traits::invalid()) noexcept { // CLOSE+TAKE
		if (h_ != Traits::invalid()) Traits::release(h_);
		h_ = h;
	}

	explicit operator bool() const noexcept {
		return h_ != Traits::invalid();
	}
};

using FilePtr = Handle<FileTraits>;
using MemPtr = Handle<HeapTraits>;
using Lock = Handle<MutexTraits>;

// ---------- factory: where validation lives ----------
[[nodiscard]] FilePtr open_file(const char *path, const char *mode) {
	std::FILE *f = std::fopen(path, mode);
	if (!f) throw std::system_error(errno, std::generic_category(), path);
	std::cout << "    [fopen " << path << "]\n";
	return FilePtr{f};
}

// borrowing in action: takes the RAW token, uses it, does NOT close
void append_line(std::FILE *borrowed, const char *text) {
	std::fputs(text, borrowed);
	std::fputc('\n', borrowed);
}

[[nodiscard]] Lock lock_mutex(std::mutex& m) {
	m.lock();
	std::cout << " [Lock" << &m << "]\n";
	return Lock{(&m)};
}

int main() {
	std::cout << "=== 1. One engine, two resources ===\n";
	{
		FilePtr log = open_file("handle_demo.txt", "w");
		append_line(log.get(), "RAII engine at work"); // get() = borrow

		MemPtr buf{new int[5]{1, 2, 3, 4, 5}}; // same engine, heap
		long sum = 0;
		for (int i = 0; i < 5; ++i) sum += buf.get()[i];
		std::cout << "  sum through borrowed token = " << sum << "\n";
	} // both released, reverse order, zero cleanup lines

	std::cout << "\n=== 2. Exception path (same engine) ===\n";
	try {
		FilePtr f = open_file("handle_demo.txt", "w");
		append_line(f.get(), "will never land");
		throw std::runtime_error("boom");
	} catch (const std::exception &e) {
		std::cout << "  caught: " << e.what() << " (fclose already ran, above)\n";
	}

	std::cout << "\n=== 3. release(): ownership OUT, dtor becomes no-op ===\n";
	std::FILE *raw; {
		FilePtr f = open_file("handle_demo.txt", "w");
		raw = f.release(); // hand out + forget
		std::cout << "  after release: bool(f) = " << bool(f) << "\n";
	} // <- dtor: empty, prints NOTHING
	std::cout << "  scope ended, no [fclose] above: wrapper no longer owns\n";
	std::fclose(raw); // ...so WE close it manually (we own it now)

	std::cout << "\n=== 4. reset(): close-then-take, the workhorse ===\n"; {
		MemPtr m{new int[3]};
		std::cout << "  holding " << m.get() << "\n";
		m.reset(new int[7]); // releases old, takes new
		std::cout << "  now holding " << m.get() << " (old freed above)\n";
		m.reset(); // just closes
		std::cout << "  after reset(): bool(m) = " << bool(m) << "\n";
	}

	std::cout << "\n=== 5. Factory throws: nothing existed ===\n";
	try {
		FilePtr nope = open_file("/no/such/dir/x.txt", "r");
	} catch (const std::system_error &e) {
		std::cout << "  " << e.code().message() << " - no wrapper, no cleanup owed\n";
	}

	std::cout << "\n=== 6. Test the mutex and lock ===\n";
	{
		std::mutex m;
		auto my_lock = lock_mutex(m);
		std::cout << "  [Working inside the Critical Section safely...]\n";
	}
	return 0;
}
