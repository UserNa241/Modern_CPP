// lesson_2_7_stress_test.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread lesson_2_7_stress_test.cpp -o stress && ./stress
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <random>

std::mutex mt;

// 1. The Account Structure
struct Account {
	std::string name;
	int balance;

	Account(std::string n, int b) : name(std::move(n)), balance(b) {}
};

struct Bank {
	std::vector<Account> accounts;
	std::mutex mtx;

	Bank() : accounts{ {"Alice", 1000}, {"Bob", 1000} } {}

	void transfer(int from_idx, int to_idx, int amount) {
		// Lock the ENTIRE bank. No deadlocks possible because there is only 1 lock!
		std::lock_guard<std::mutex> lock(mtx);
		accounts[from_idx].balance -= amount;
		accounts[to_idx].balance += amount;
	}
};

int main() {

	std::vector<std::unique_ptr<Account>> bank;

	Bank my_bank;

	{
		std::jthread t1([&] {
			for (int i = 0; i < 100; i++) {
				my_bank.transfer(0,1,5);
			}
		});

		std::jthread t2([&] {
			for (int i = 0; i < 100; i++) {
				my_bank.transfer(1,0,5);
			}
		});
	}

	std::cout << "Alice: " << my_bank.accounts[0].balance << '\n';
	std::cout << "Bob: " << my_bank.accounts[1].balance << '\n';
	return 0;
}
