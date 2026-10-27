#include <iostream>

#include <random>
int main() {

	auto upper = [](char c) {
		return toupper(c);
	};

	upper('a');
	upper('b');

	return 0;
}
