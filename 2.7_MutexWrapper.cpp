// lesson_2_7_mutex_wrappers.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread lesson_2_7_mutex_wrappers.cpp -o mwrap && ./mwrap
#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;

struct Account {
    std::string name;
    int         balance;
    std::mutex  mtx;
    Account(std::string n, int b) : name(std::move(n)), balance(b) {}
};

// ---------- 1. raw lock: the resource leaks on throw ----------
void unsafe_credit(Account& a, int amount) {
    a.mtx.lock();
    a.balance += amount;
    std::cout << "  [unsafe] " << a.name << " now " << a.balance << "\n";
    throw std::runtime_error("unsafe_credit exploded");
    a.mtx.unlock();                                    // never reached
}

// ---------- 2. lock_guard: unlock is the destructor ----------
void guarded_credit(Account& a, int amount) {
    std::lock_guard<std::mutex> g(a.mtx);
    a.balance += amount;
    std::cout << "  [guard]  " << a.name << " now " << a.balance << "\n";
    throw std::runtime_error("guarded_credit exploded");
}

// ---------- 3. unique_lock knobs ----------
void unique_credit_then_log(Account& a, int amount) {
    std::unique_lock<std::mutex> ul(a.mtx);
    a.balance += amount;
    int snap = a.balance;
    ul.unlock();                                       // drop lock BEFORE I/O
    std::cout << "  [unique] unlocked, then logged snap=" << snap << "\n";
}

void unique_deferred(Account& a) {
    std::unique_lock<std::mutex> ul(a.mtx, std::defer_lock);
    std::cout << "  [unique] deferred owns_lock=" << ul.owns_lock() << "\n";
    ul.lock();
    std::cout << "  [unique] after lock  owns_lock=" << ul.owns_lock()
              << "  balance=" << a.balance << "\n";
}

// ---------- 4. deadlock on purpose (timed so we don't hang the lesson) ----------
std::timed_mutex mA;
std::timed_mutex mB;

void deadlock_ab() {
    mA.lock();
    std::this_thread::sleep_for(40ms);                 // let the other thread grab mB
    const bool got = mB.try_lock_for(80ms);
    std::cout << "  [A] holds mA, try mB -> " << (got ? "got\n" : "TIMEOUT (would deadlock)\n");
    if (got) mB.unlock();
    mA.unlock();
}

void deadlock_ba() {
    mB.lock();
    std::this_thread::sleep_for(40ms);
    const bool got = mA.try_lock_for(80ms);
    std::cout << "  [B] holds mB, try mA -> " << (got ? "got\n" : "TIMEOUT (would deadlock)\n");
    if (got) mA.unlock();
    mB.unlock();
}

// ---------- 5. alphabet / address-order trick ----------
void lock_ordered(std::mutex& x, std::mutex& y) {
    std::mutex* first  = &x < &y ? &x : &y;            // lower address first
    std::mutex* second = &x < &y ? &y : &x;
    first->lock();
    second->lock();
    std::cout << "  [alpha] locked in address order "
              << first << " then " << second << "\n";
    second->unlock();
    first->unlock();
}

// ---------- 6. scoped_lock: std::lock + RAII in one type ----------
void transfer_scoped(Account& from, Account& to, int amount) {
    std::scoped_lock both(from.mtx, to.mtx);           // order of args does not matter
    from.balance -= amount;
    to.balance   += amount;
    std::cout << "  [scoped] " << from.name << " -> " << to.name
              << " " << amount << "\n";
}

int main() {
    Account alice("alice", 100);
    Account bob  ("bob",   100);

    std::cout << "=== 1. Raw lock + throw: mutex stays LOCKED ===\n";
    try { unsafe_credit(alice, 10); }
    catch (const std::exception& e) { std::cout << "  caught: " << e.what() << "\n"; }
    std::cout << "  try_lock: "
              << (alice.mtx.try_lock() ? "GOT IT\n" : "FAILED (lock leaked)\n");
    alice.mtx.unlock();                                // emergency: continue the lesson
    std::cout << "  (force-unlocked so later sections can run)\n";

    std::cout << "\n=== 2. lock_guard + throw: destructor unlocked ===\n";
    try { guarded_credit(alice, 10); }
    catch (const std::exception& e) { std::cout << "  caught: " << e.what() << "\n"; }
    std::cout << "  try_lock: "
              << (alice.mtx.try_lock() ? "GOT IT (dtor ran)\n" : "FAILED\n");
    alice.mtx.unlock();

    std::cout << "\n=== 3. unique_lock: unlock early + defer ===\n";
    unique_credit_then_log(alice, 5);
    unique_deferred(alice);

    std::cout << "\n=== 4. Deadlock on purpose (timed_mutex, opposite order) ===\n";
    {
        std::jthread t1(deadlock_ab);
        std::jthread t2(deadlock_ba);
    }

    std::cout << "\n=== 5. Alphabet trick: always lock lower address first ===\n";
    {
        std::jthread t1([&] { lock_ordered(alice.mtx, bob.mtx); });
        std::jthread t2([&] { lock_ordered(bob.mtx, alice.mtx); });
    }

    std::cout << "\n=== 6. scoped_lock both directions: no deadlock ===\n";
    {
        std::jthread t1([&] { transfer_scoped(alice, bob, 7); });
        std::jthread t2([&] { transfer_scoped(bob, alice, 3); });
    }
    std::cout << "  balances alice=" << alice.balance
              << " bob=" << bob.balance << "\n";
    return 0;
}
