#include <iostream>
#include <stdexcept>
#include <cstddef>
#include <chrono>

class TimerGuard {
public:
	explicit TimerGuard (const char* data) : data_(new char[std::strlen(data)+1]) {
		std::strcpy(data_,data);
		now = std::chrono::steady_clock::now();
	};

	~TimerGuard() noexcept {
		end = std::chrono::steady_clock::now();
		std::cout << data_ << " took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - now) << '\n';
		delete[] data_;
	}

	TimerGuard (const TimerGuard&) = delete;
	TimerGuard operator=(const TimerGuard&) = delete;

private:
	char *data_;
	std::chrono::time_point<std::chrono::steady_clock>now, end;
};

int main() {
	TimerGuard T("Arena");
	for (int i = 0; i < 10000000; i++) {}
	return 0;
}