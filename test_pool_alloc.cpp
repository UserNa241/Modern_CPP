#include <iostream>

class Pool {
	std::unique_ptr<int[]> data_;
	std::unique_ptr<bool[]> in_use_;
	std::size_t size_;
	std::size_t active_;

	void release(int* ptr) {
		int index = ptr - data_.get();
		in_use_[index] = false;
		--active_;
	}

public:
	struct Releaser {
		Pool *parent;

		void operator() (int* ptr) const {
			if (parent && ptr) {
				parent -> release(ptr);
			}
		}
	};
	using Handle = std::unique_ptr<int, Releaser>;

	Pool(int N) :	data_(std::make_unique<int[]>(N)),
					in_use_(std::make_unique<bool[]>(N)),
					size_(N), active_(0) {}

	Handle acquire() {
		for (std::size_t i = 0; i < size_; ++i) {
			if (!in_use_[i]) {
				in_use_[i] = true;
				++active_;
				return {&data_[i],Releaser{this}};
			}
		}
		throw std::runtime_error("Pool is full");
	}

	int active_count() const {
		return active_;
	}

};

int main() {

	Pool pool(10);

	auto p1 = pool.acquire();
	auto p2 = pool.acquire();
	auto p3 = pool.acquire();

	std::cout << pool.active_count() << '\n';
	{
		auto p4 = pool.acquire();
		std::cout << "Count: " << pool.active_count() << '\n';
	}

	auto p4 = pool.acquire();
	std::cout << "Count: " << pool.active_count() << '\n';

	return 0;
}