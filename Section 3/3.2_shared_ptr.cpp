// lesson_3_2_shared_weak.cpp — portable, standard C++ only
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror lesson_3_2_shared_weak.cpp -o sw && ./sw
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

// ---------- instrumented Widget: prints birth & death ----------
struct Widget {
	std::string name;

	explicit Widget(std::string n) : name(std::move(n)) {
		std::cout << "    + Widget '" << name << "'\n";
	}

	~Widget() {
		std::cout << "    - Widget '" << name << "'\n";
	}
};

// ---------- the cycle, leaking and fixed ----------
struct BadNode { // shared both ways = leak
	std::string name;
	std::shared_ptr<BadNode> peer;
	explicit BadNode(std::string n) : name(std::move(n)) {}

	~BadNode() {
		std::cout << "    - BadNode '" << name << "'\n";
	}
};

struct GoodNode { // one edge weak = no leak
	std::string name;
	std::weak_ptr<GoodNode> peer; // OBSERVE, don't own
	explicit GoodNode(std::string n) : name(std::move(n)) {}

	~GoodNode() {
		std::cout << "    - GoodNode '" << name << "'\n";
	}
};

// ---------- cache/observer pattern ----------
struct ImageCache {
	std::vector<std::weak_ptr<Widget> > entries; // observers: don't keep images alive
	void insert(const std::shared_ptr<Widget> &w) {
		entries.push_back(w);
	}

	void sweep() { // drop dead entries, report alive ones
		std::cout << "    cache sweep:\n";
		for (auto &e: entries) {
			if (auto locked = e.lock())
				std::cout << "      alive: '" << locked->name << "'\n";
			else
				std::cout << "      expired entry (dropped)\n";
		}
		std::erase_if(entries, [](auto &e)
		{
			return e.expired();
		});
	}
};

int main() {

	std::cout << "=== 1. Birth, sharing, and the last-owner-frees rule ===\n";
	{
		auto a = std::make_shared<Widget>("primary"); // strong=1
		std::cout << "  use_count=" << a.use_count() << "\n";
		{
			auto b = a; // strong=2 (copy)
			auto c = a; // strong=3
			std::cout << "  after 2 copies: use_count=" << a.use_count() << "\n";
		} // c,b die: strong=1
		std::cout << "  inner scope closed: use_count=" << a.use_count()
				<< " (still alive - a owns it)\n";
	} // a dies: strong=0 -> destroyed
	std::cout << "  outer scope closed: object freed (see '-' above)\n";

	std::cout << "\n=== 2. weak_ptr observes without keeping alive ===\n";
	std::weak_ptr<Widget> observer;
	{
		auto sp = std::make_shared<Widget>("watched");
		observer = sp;
		std::cout << "  inside: expired=" << observer.expired()
				<< ", use_count=" << observer.use_count() << "\n";
	}
	std::cout << "  outside: expired=" << observer.expired()
			<< " (object already freed - weak did NOT extend its life)\n";

	std::cout << "\n=== 3. THE CYCLE: leak, then fix ===\n";
	{
		auto n1 = std::make_shared<BadNode>("left");
		auto n2 = std::make_shared<BadNode>("right");
		n1->peer = n2;
		n2->peer = n1; // cycle: strong counts never drain
		std::cout << "  BadNodes going out of scope... (watch: NO destructors print)\n";
	} // LEAK: both silent
	std::cout << "  ^ silence = leak. Now the fixed version:\n";
	{
		auto n1 = std::make_shared<GoodNode>("L");
		auto n2 = std::make_shared<GoodNode>("R");
		n1->peer = n2; // n1 OWNS n2 (shared)
		n2->peer = n1; // n2 OBSERVES n1 (weak)
		std::cout << "  GoodNodes closing scope:\n";
	} // no cycle: both destroyed, in order
	std::cout << "  ^ destructors printed = freed\n";

	std::cout << "\n=== 4. lock(): observe-and-hold atomically ===\n";
	std::weak_ptr<Widget> w; {
		auto sp = std::make_shared<Widget>("locked");
		w = sp;
		if (auto locked = w.lock()) { // alive? then hold it
			std::cout << "  locked: '" << locked->name << "', use_count now "
					<< sp.use_count() << "\n";
		}
	}
	if (auto locked = w.lock()) std::cout << "  (never prints)\n";
	else std::cout << "  lock after death = empty shared_ptr, handled safely\n";

	std::cout << "\n=== 5. Cache of weak observers ===\n";
	ImageCache cache;
	{
		auto cat = std::make_shared<Widget>("cat.png");
		auto dog = std::make_shared<Widget>("dog.png");
		cache.insert(cat);
		cache.insert(dog);
		cache.sweep(); // both alive
	} // cat, dog die
	cache.sweep(); // entries expired
	cache.sweep(); // cache cleaned itself

	std::cout << "\n=== 6. Aliasing constructor + shared_from_this ===\n";
	struct Owner : std::enable_shared_from_this<Owner> {
		Widget member{"owned-member"};

		std::shared_ptr<Widget> member_view() {
			return {shared_from_this(), &member}; // ALIASING: keeps Owner alive
		}
	};

	std::weak_ptr<Widget> member_ref;
	{
		auto owner = std::make_shared<Owner>();
		member_ref = owner->member_view(); // view into owner's member
		std::cout << "  alias acquired (owner use_count=" << owner.use_count() << ")\n";
	}
	std::cout << "  owner gone: member view expired=" << member_ref.expired()
			<< " (alias kept WHOLE owner alive until now)\n";
	return 0;
}
