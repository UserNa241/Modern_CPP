#include <iostream>

int main() {
	std::cout << "Starting unsafe memory test..." << std::endl;

	// Create an array of 5 elements
	int* array = new int[5]{1, 2, 3, 4, 5};

	// Intentionally write far past the allocated memory buffer
	// This will trigger an ASan global-buffer-overflow / heap-use-after-free
	array[10] = 999;

	std::cout << "Value at index 10: " << array[10] << std::endl;

	delete[] array;
	return 0;
}
