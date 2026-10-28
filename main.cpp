#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include <type_traits>
#include <vector>

struct MoveOK {
	static inline int copies = 0;
	static inline int moves = 0;
	MoveOK() = default;
	MoveOK(MoveOK&&) noexcept {++moves;}
	MoveOK& operator= (MoveOK&&) noexcept {++moves;}
	MoveOK(MoveOK&) = delete;
	MoveOK& operator=(MoveOK&) = delete;
};

struct MoveNotOk {
	static inline int copies = 0;
	static inline int moves = 0;
	MoveNotOk() = default;
	MoveNotOk(MoveNotOk&&)  {++moves;}
	MoveNotOk& operator= (MoveNotOk&&)  {++moves;}
	MoveNotOk(MoveNotOk&) = delete;
	MoveNotOk& operator=(MoveNotOk&) = delete;
};

struct MoveNotOkCopyOk {
	static inline int copies = 0;
	static inline int moves = 0;
	MoveNotOkCopyOk() = default;
	MoveNotOkCopyOk(const MoveNotOkCopyOk&) {++moves;}
	MoveNotOkCopyOk& operator=(const MoveNotOkCopyOk&) {++moves;}
	MoveNotOkCopyOk(MoveNotOkCopyOk&&) {++copies;}
	MoveNotOkCopyOk& operator=(MoveNotOkCopyOk&&) {++copies;}
};

int main() {

	static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<MoveOK&>())),MoveOK&&>);
	static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<MoveNotOk&>())),MoveNotOk&&>);
	static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<MoveNotOkCopyOk&>())), const MoveNotOkCopyOk&>);

	std::vector<MoveOK> vA;
	for (int i = 0; i < 8; i++) {
		vA.push_back(MoveOK{});
	}
	std::cout << "MoveOK" << '\n';
	std::cout << "Copies	:" << MoveOK::copies << '\n';
	std::cout << "Moves		:" << MoveOK::moves << '\n';

	std::vector<MoveNotOk> vB;
	for (int i = 0; i < 8; i++) {
		vB.push_back(MoveNotOk{});
	}
	std::cout << "MoveNotOK" << '\n';
	std::cout << "Copies	:" << MoveNotOk::copies << '\n';
	std::cout << "Moves		:" << MoveNotOk::moves << '\n';

	std::vector<MoveNotOkCopyOk> vC;
	for (int i = 0; i < 8; i++) {
		vC.push_back(MoveNotOkCopyOk{});
	}
	std::cout << "MoveNotOKCopyOk" << '\n';
	std::cout << "Copies	:" << MoveNotOkCopyOk::copies << '\n';
	std::cout << "Moves		:" << MoveNotOkCopyOk::moves << '\n';

	return 0;
}
