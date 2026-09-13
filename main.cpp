#include <iostream>
#include <memory>
#include <string>

struct Node {
	std::string name_;
	std::shared_ptr<Node> next;
	std::weak_ptr<Node> prev;

	explicit Node(const std::string name) : name_(std::move(name)) {
		std::cout << "	+Node "<< name_ <<'\n';
	}

	~Node() {
		std::cout << " -Node "<< name_ << '\n';
	}


};

int main() {

	auto a = std::make_shared<Node>("A");
	auto b = std::make_shared<Node>("B");
	auto c = std::make_shared<Node>("C");

	a -> next = b;

	b -> next = c;
	b -> prev = a;

	c -> prev = b;

	std::cout << "\nUse counts before reset:\n";
	std::cout << "a:	" << a.use_count() << '\n';
	std::cout << "b:	" << b.use_count() << '\n';
	std::cout << "c:	" << c.use_count() << '\n';

	for (auto p = a; p; p = p->next) {
		std::cout << p->name_ << '\n';
	}

	for (auto p = c; p; ) {
		std::cout << p->name_ << '\n';
		p = p->prev.lock();
	}

	a.reset();
	std::cout << "\nUse counts after reset:\n";
	std::cout << "a:	" << a.use_count() << '\n';
	std::cout << "b:	" << b.use_count() << '\n';
	std::cout << "c:	" << c.use_count() << '\n';

	b.reset();
	c.reset();

	return 0;
}
