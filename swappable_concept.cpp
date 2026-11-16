#include <iostream>
#include <utility>

template <class  T>
concept Swappable =
	std::is_move_assignable_v<T> &&
	std::is_move_constructible_v<T> &&
	requires ( T& a, T& b) {
	std::swap(a,b);
	};

template <Swappable T>
void swap_or_copy(T& a, T& b) {
	std::swap(a,b);
}

struct NoCopyNoMove {
	NoCopyNoMove() = default;
	NoCopyNoMove(const NoCopyNoMove&) = delete;
	NoCopyNoMove(NoCopyNoMove&&) = delete;
};

static_assert(Swappable<int>);
static_assert(Swappable<std::string>);
static_assert(!Swappable<NoCopyNoMove>);

int main() {
	NoCopyNoMove x,y;

	return 0;
}
