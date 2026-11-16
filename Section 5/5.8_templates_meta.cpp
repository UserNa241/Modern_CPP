// lesson_5_8_templates_meta.cpp — template metaprogramming patterns: type lists, sizeof..., static strings
//
// ONE idea: templates are a compile-time language. You can build data structures
// out of types (type lists), compute over them at compile time (fold over a type
// list), and even construct strings that exist only in the compiler — all before
// main() runs.
//
// Build: g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror lesson_5_8_templates_meta.cpp -o meta && ./meta

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>

// ============================================================
// 1. TYPE LIST — a compile-time list of types
// ============================================================

template <class... Ts>
struct TypeList {
    static constexpr std::size_t size = sizeof...(Ts);
};

// Head / Tail decomposition
template <class TList> struct Head;   // undefined on purpose
template <class First, class... Rest>
struct Head<TypeList<First, Rest...>> { using type = First; };

template <class TList> struct Tail;   // undefined on purpose
template <class First, class... Rest>
struct Tail<TypeList<First, Rest...>> { using type = TypeList<Rest...>; };

static_assert(std::is_same_v<Head<TypeList<int, double, char>>::type, int>);
static_assert(std::is_same_v<Tail<TypeList<int, double, char>>::type, TypeList<double, char>>);
static_assert(TypeList<int, double, char>::size == 3);
static_assert(TypeList<>::size == 0);

// ============================================================
// 2. MAP over a type list — transform every type
//    Specialized per transformation (simplest reliable pattern).
// ============================================================

template <class TList> struct TypeMapAddConst;  // undefined

template <class... Ts>
struct TypeMapAddConst<TypeList<Ts...>> {
    using type = TypeList<const Ts...>;
};

using Mapped = TypeMapAddConst<TypeList<int, double, char>>::type;
static_assert(std::is_same_v<Mapped, TypeList<const int, const double, const char>>);

// Prepend a single type to the front of a type list
template <class T, class TList> struct Prepend;  // undefined
template <class T, class... Ts>
struct Prepend<T, TypeList<Ts...>> { using type = TypeList<T, Ts...>; };

// ============================================================
// 3. FILTER over a type list — keep only integral types
//    Specialized per predicate (simplest reliable pattern).
// ============================================================

template <class TList> struct TypeFilterIntegral;  // undefined

template <>
struct TypeFilterIntegral<TypeList<>> { using type = TypeList<>; };

template <class First, class... Rest>
struct TypeFilterIntegral<TypeList<First, Rest...>> {
    using RestFiltered = typename TypeFilterIntegral<TypeList<Rest...>>::type;
    using type = std::conditional_t<std::is_integral_v<First>,
                                    typename Prepend<First, RestFiltered>::type,
                                    RestFiltered>;
};

using Filtered = TypeFilterIntegral<TypeList<int, double, char, std::string>>::type;
static_assert(std::is_same_v<Filtered, TypeList<int, char>>);

// ============================================================
// 4. STATIC STRING — a string that exists only at compile time
// ============================================================

template <char... Cs>
struct StaticString {
    static constexpr std::size_t size = sizeof...(Cs);
    static constexpr char data[sizeof...(Cs) + 1] = {Cs..., '\0'};
    static constexpr const char* c_str() { return data; }
};

static_assert(StaticString<'h','e','l','l','o'>::size == 5);

// Compile-time string concatenation
template <char... A, char... B>
constexpr auto concat(StaticString<A...>, StaticString<B...>) {
    return StaticString<A..., B...>{};
}

constexpr StaticString<'h','e','l','l','o'> hello;
constexpr StaticString<'w','o','r','l','d'> world;
constexpr StaticString hello_world = concat(hello, world);

static_assert(hello_world.size == 10);
static_assert(hello_world.c_str()[0] == 'h');
static_assert(hello_world.c_str()[5] == 'w');

// ============================================================
// 5. sizeof... at work: count types in a variadic template
// ============================================================

template <class... Args>
constexpr std::size_t count_types() {
    return sizeof...(Args);
}

static_assert(count_types<>() == 0);
static_assert(count_types<int>() == 1);
static_assert(count_types<int, double, char>() == 3);

int main() {
    std::cout << "=== Type list ===\n";
    std::cout << "  TypeList<int, double, char>::size = " << TypeList<int, double, char>::size << "\n";
    std::cout << "  head = int, tail size = " << Tail<TypeList<int, double, char>>::type::size << "\n";

    std::cout << "\n=== TypeMap: add const to every type ===\n";
    std::cout << "  mapped: const int, const double, const char\n";

    std::cout << "\n=== TypeFilter: keep only integral types ===\n";
    std::cout << "  filtered: int, char\n";

    std::cout << "\n=== StaticString ===\n";
    std::cout << "  hello: " << hello.c_str() << " (size " << hello.size << ")\n";
    std::cout << "  world: " << world.c_str() << " (size " << world.size << ")\n";
    std::cout << "  concat: " << hello_world.c_str() << " (size " << hello_world.size << ")\n";

    std::cout << "\n=== sizeof... ===\n";
    std::cout << "  count_types<>() = " << count_types<>() << "\n";
    std::cout << "  count_types<int, double, char>() = " << count_types<int, double, char>() << "\n";

    return 0;
}
