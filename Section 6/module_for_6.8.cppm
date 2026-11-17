// Lesson 6.8 — Modules deep dive — MODULE INTERFACE
// File: geo_file_for_6_8.cppm
// Build step 1: g++ -std=c++20 -fmodules-ts -c geo_file_for_6_8.cppm -o geo_file_for_6_8.o
// Build step 2: g++ -std=c++20 -fmodules-ts lesson_6_8_modules.cpp geo_file_for_6_8.o -o lesson_6_8_modules
//
// This file defines the 'geometry' module interface. Everything marked 'export'
// is visible to importers; everything else is module-private.
//
// Run: ./lesson_6_8_modules

module;

# include <iostream>
# include <cmath>

export module geometry;
//===================================================================
// Exported types — visible to every importer
//===================================================================

export struct Point {
	double x;
	double y;
};

//===================================================================
// Exported functions — visible to every importer
//===================================================================

export double distance(const Point& a, const Point& b) {
	double dx = a.x - b.x;
	double dy = a.y - b.y;
	return std::sqrt(dx * dx + dy * dy);
}

export Point midpoint(const Point& a, const Point& b) {
	return {(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
}

//===================================================================
// Module-private — NOT exported, invisible to importers
//===================================================================

namespace detail {
	double clamp(double v, double lo, double hi) {
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}
}
