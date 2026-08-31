#include <iostream>

int main() {
	int* p = new int[5];

	volatile int index = 5;
	p[index] = 42;

	std::cout << p[index] << '\n';

	delete[] p;
}