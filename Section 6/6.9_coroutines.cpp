// Lesson 6.9 — Coroutines deep dive
//
// Topics:
//   co_yield — produce values lazily                               (C++20)
//   co_await — suspend/resume on an awaitable                      (C++20)
//   co_return — return a value from a coroutine                    (C++20)
//   promise_type — bridges coroutine and its return object         (C++20)
//   Lazy generator — values computed on demand, not upfront        (C++20)
//
// Build (GCC requires -fcoroutines):
//   g++ -std=c++23 -fcoroutines -Wall -Wextra -Wpedantic -Werror lesson_6_9_coroutines.cpp -o coro && ./coro
//
// Note: -fcoroutines is a GCC implementation flag. Coroutines are a C++20
// language feature; std::generator<T> is the C++23 standard wrapper.
//
// One idea: coroutines let a function suspend and resume, enabling lazy,
// pull-based data production instead of eager, push-based computation.

#include <coroutine>
#include <iostream>

//===================================================================
// 1. co_yield + promise_type — lazy generator
//===================================================================

// A minimal generator that yields ints one at a time. The coroutine
// suspends after each yield and resumes when the caller asks for the
// next value. Nothing is computed until requested.
struct Generator {
    struct promise_type {
        int current_value = 0;

        Generator get_return_object() {
            return Generator{this};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}
        void unhandled_exception() {}

        // Called for each co_yield — suspends and stores the value
        std::suspend_always yield_value(int v) {
            current_value = v;
            return {};
        }
    };

    using handle = std::coroutine_handle<promise_type>;
    handle h;

    explicit Generator(promise_type* p) : h(handle::from_promise(*p)) {}

    ~Generator() {
        if (h) h.destroy();
    }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    Generator(Generator&& o) noexcept : h(o.h) { o.h = nullptr; }
    Generator& operator=(Generator&& o) noexcept {
        if (h) h.destroy();
        h = o.h;
        o.h = nullptr;
        return *this;
    }

    // Resume the coroutine; returns false when it's done
    bool next() {
        h.resume();
        return !h.done();
    }

    int value() const {
        return h.promise().current_value;
    }
};

// Fibonacci as a lazy generator — each value is computed only when asked
Generator fibonacci(int n) {
    int a = 0, b = 1;
    for (int i = 0; i < n; ++i) {
        co_yield a;       // suspend here; caller gets 'a'
        int tmp = a + b;
        a = b;
        b = tmp;
    }
    // co_return void implied when falling off the end
}

//===================================================================
// 2. co_await — suspend until an awaitable is ready
//===================================================================

struct DelayedValue {
    int ms;               // pretend "ms" of delay

    bool await_ready() { return false; }   // always suspend at least once
    void await_suspend(std::coroutine_handle<>) {}
    int await_resume() { return ms; }      // the "result" of the wait
};

struct TimedTask {
    struct promise_type {
        int result = 0;

        TimedTask get_return_object() { return TimedTask{this}; }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(int v) { result = v; }
        void unhandled_exception() {}
    };

    using handle = std::coroutine_handle<promise_type>;
    handle h;

    explicit TimedTask(promise_type* p) : h(handle::from_promise(*p)) {}
    ~TimedTask() { if (h) h.destroy(); }
    TimedTask(const TimedTask&) = delete;
    TimedTask& operator=(const TimedTask&) = delete;
    TimedTask(TimedTask&& o) noexcept : h(o.h) { o.h = nullptr; }
    TimedTask& operator=(TimedTask&& o) noexcept {
        if (h) h.destroy();
        h = o.h;
        o.h = nullptr;
        return *this;
    }

    // Resume until the coroutine completes
    int run() {
        while (!h.done()) {
            h.resume();
        }
        return h.promise().result;
    }
};

TimedTask delayed_compute(int x) {
    int waited = co_await DelayedValue{100};   // suspend, then resume with 100
    co_return x + waited;                      // 40 + 100 = 140
}

//===================================================================
// helpers
//===================================================================

int main() {
    //---------------------------------------------------------------
    // co_yield: lazy fibonacci generator
    //---------------------------------------------------------------
    std::cout << "=== co_yield — lazy generator (fibonacci) ===\n";
    auto g = fibonacci(8);
    while (g.next()) {
        std::cout << g.value() << " ";
    }
    std::cout << "\n";

    //---------------------------------------------------------------
    // co_await: delayed compute
    //---------------------------------------------------------------
    std::cout << "=== co_await — delayed compute ===\n";
    auto t = delayed_compute(40);
    std::cout << "result: " << t.run() << "\n";   // 140

    return 0;
}
