// Lesson 6.8 — Modules deep dive — CONSUMER
// File: lesson_6_8_modules.cpp
// Build step 2: g++ -std=c++20 -fmodules-ts lesson_6_8_modules.cpp geo_file_for_6_8.o -o lesson_6_8_modules
//
// This consumer imports the geometry module. No #include needed — the module
// interface (geo_file_for_6_8.cppm) was compiled into geo_file_for_6_8.o + gcm.cache/.
//
// Run: ./lesson_6_8_modules

# include  <iostream>
# include  <string>

import geometry;

int main() {
	Point a{0.0, 0.0};
	Point b{3.0, 4.0};

	std::cout << "distance((" << a.x << "," << a.y << "), ("
			  << b.x << "," << b.y << ") = " << distance(a, b) << "\n";

	Point m = midpoint(a, b);
	std::cout << "midpoint = (" << m.x << ", " << m.y << ")\n";

	std::string msg = "Modules work!";
	std::cout << msg << "\n";

	// detail::clamp is module-private — not accessible from here.
	// Uncommenting would be a compile error:
	// double c = detail::clamp(5.0, 0.0, 10.0);

	return 0;
}