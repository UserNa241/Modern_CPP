// lesson_5_3_variadic.cpp — variadic templates: parameter packs and pack expansion
//
// ONE idea: a parameter pack (Args... or args...) holds ZERO or more types/values
// at once, and pack expansion applies a pattern to every element of the pack.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_5_3_variadic.cpp -o var && ./var

#include <iostream>
#include <string>
#include <type_traits>

// 1. Print each argument on its own line. Recursion: the classic way.
//    Base case: one argument.
template <class T>
void print_one(T t) {
    std::cout << "  " << t << "\n";
}

// Recursive case: print first, then recurse on the rest.
template <class T, class... Rest>
void print_recursive(T first, Rest... rest) {
    std::cout << "  " << first << "\n";
    if constexpr (sizeof...(rest) > 0) {
        print_recursive(rest...);   // pack expansion: Rest... and rest... shrink by one
    }
}

// 2. Fold expression (C++17): apply an operator across the whole pack at once.
//    Sum all arguments, no recursion, no base case.
template <class... Args>
auto sum_all(Args... args) {
    return (0 + ... + args);   // binary left fold with init: ((0 + a1) + a2) + ...; empty pack -> 0
}

// 3. Count how many arguments are equal to a given value.
template <class T, class... Args>
int count_equal(T value, Args... args) {
    return ((args == value) + ...);   // each comparison yields bool -> 0 or 1, then summed
}

// 4. Pack expansion inside a type: build a std::tuple from the pack.
#include <tuple>
template <class... Args>
std::tuple<Args...> make_tuple_from(Args... args) {
    return std::tuple<Args...>(args...);
}

// 5. Variadic class template: a container holding one value of each type.
template <class... Types>
struct HeteroContainer;

template <class First, class... Rest>
HeteroContainer(First, Rest...) -> HeteroContainer<First, Rest...>;

template <>
struct HeteroContainer<> {   // empty pack specialization
    void describe() const { std::cout << "    (empty)\n"; }
};

template <class First, class... Rest>
struct HeteroContainer<First, Rest...> {
    First first;
    HeteroContainer<Rest...> rest;
    HeteroContainer(First f, Rest... r) : first(f), rest(r...) {}
    void describe() const {
        std::cout << "    head: " << first << "\n";
        rest.describe();
    }
};

int main() {
    std::cout << "=== 1. Recursive print ===\n";
    print_recursive(1, 2.5, std::string("three"), 4);

    std::cout << "\n=== 2. Fold expression: sum_all ===\n";
    std::cout << "  sum_all(1, 2, 3, 4, 5) = " << sum_all(1, 2, 3, 4, 5) << "\n";
    std::cout << "  sum_all(10, 20) = " << sum_all(10, 20) << "\n";
    std::cout << "  sum_all() = " << sum_all() << "\n";   // empty pack: 0 for +

    std::cout << "\n=== 3. count_equal ===\n";
    std::cout << "  count_equal(7, 1, 7, 2, 7, 3) = " << count_equal(7, 1, 7, 2, 7, 3) << "\n";
    std::cout << "  count_equal(9, 1, 2, 3) = " << count_equal(9, 1, 2, 3) << "\n";

    std::cout << "\n=== 4. make_tuple_from (pack expansion in a type) ===\n";
    auto t = make_tuple_from(1, 2.5, std::string("hi"));
    static_assert(std::is_same_v<decltype(t), std::tuple<int, double, std::string>>);
    std::cout << "  tuple: " << std::get<0>(t) << ", " << std::get<1>(t)
              << ", " << std::get<2>(t) << "\n";

    std::cout << "\n=== 5. HeteroContainer (variadic class template) ===\n";

    HeteroContainer<int, std::string, double> hc{42, "hello", 3.14};
    hc.describe();
    HeteroContainer<> empty;
    empty.describe();

    HeteroContainer h2{42, "hello", 3.14};

    return 0;
}
