
#include <functional>
#include <vector>
#include <type_traits>
#include <iostream>

int main() {

	std::move_only_function<void()> a = []{(std::puts("a-target")); };

	std::move_only_function<void()> b = std::move(a);

	std::printf("after move: b= %d a=%d\n", (bool)b, (bool)a);

	b();

	std::printf("sizeof(std::function<void()>)          = %zu\n", sizeof(std::function<void()>));
	std::printf("sizeof(std::move_only_function<void()>) = %zu\n", sizeof(std::move_only_function<void()>));

}
