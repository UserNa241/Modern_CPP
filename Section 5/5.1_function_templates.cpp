// lesson_5_1_function_templates.cpp — function templates & class templates
//
// ONE idea: function templates DEDUCE T from call arguments; class templates
// do NOT (without CTAD) — explicit template arguments override both.
//
// Build: cmake --build cmake-build-debug --target 5.01_function_templates
// Run:   .\cmake-build-debug\5.01_function_templates.exe

#include <iostream>
#include <string>
#include <type_traits>

// Function template: T is DEDUCED from the call arguments.
template <class T>
T add(T a, T b) {
	return a + b;
}

// Class template: NO automatic deduction (pre-CTAD rule; CTAD is C++17+).
template <class T>
struct Box {
	T value;
	explicit Box(T v) : value(v) {}
	T get() const { return value; }
};

int main() {
	// 1. Deduction works when all arguments agree on one type.
	auto r1 = add(1, 2);
	static_assert(std::is_same_v<decltype(r1), int>);  // T deduced as int
	std::cout << "add(1, 2) = " << r1 << "   (T deduced as int)\n";

	// 2. Deduction FAILS when the arguments disagree.
	// auto r2 = add(1, 2.0);   // ERROR: cannot deduce T (int vs double)

	// 3. Explicit template argument overrides deduction.
	auto r2 = add<int>(1, 2.0);
	static_assert(std::is_same_v<decltype(r2), int>);  // T forced to int
	std::cout << "add<int>(1, 2.0) = " << r2 << "   (T forced to int; 2.0 -> 2)\n";

	// 4. Class template: explicit argument required.
	Box<int> b{42};
	static_assert(std::is_same_v<decltype(b), Box<int>>);
	std::cout << "b.get() = " << b.get() << "   (Box<int>, explicit)\n";

	// Box bb{42};   // ERROR: no automatic deduction for class templates here

	return 0;
}
