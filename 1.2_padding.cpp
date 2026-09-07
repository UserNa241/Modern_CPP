// lesson_1_2_alignment_padding.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_1_2_alignment_padding.cpp -o lesson12 && ./lesson12
#include <iostream>
#include <cstddef>    // offsetof, size_t

// ---------------- The victims ----------------
struct Bad    { char c; int i; char c2; };        // naive order
struct Good   { int i; char c; char c2; };        // reordered: large -> small
struct Ugly   { char c; double d; char c2; };     // double sandwiched by chars
struct Better { double d; char c; char c2; };     // reordered

struct alignas(64) CacheLine { int hot; };        // one hot int, alone on its line

// ------------- Layout locked into the BUILD -------------
// If someone reorders a member, these fire and compilation FAILS.
static_assert(alignof(Bad) == 4 && sizeof(Bad) == 12);
static_assert(offsetof(Bad,  c ) == 0);
static_assert(offsetof(Bad,  i ) == 4);           // 3 bytes of hole before i
static_assert(offsetof(Bad,  c2) == 8);           // then 3 bytes tail padding

static_assert(sizeof(Good) == 8 && offsetof(Good, c) == 4 && offsetof(Good, c2) == 5);

static_assert(alignof(Ugly) == 8 && sizeof(Ugly) == 24);    // 8/24 = 33% data
static_assert(sizeof(Better) == 16);                        // 8/16 = 50% ... wait and see
static_assert(alignof(CacheLine) == 64 && sizeof(CacheLine) == 64);

// ------------- Byte-map printer -------------
struct Field { std::size_t offset, size; char tag; const char* name; };

void print_layout(const char* title, const Field f[], std::size_t n, std::size_t total) {
    std::cout << "  " << title << "   size = " << total << " bytes\n";
    for (std::size_t byte = 0; byte < total; ++byte) {
        if (byte % 16 == 0) std::cout << "    ";
        char tag = '.';                                   // '.' = padding byte
        for (std::size_t k = 0; k < n; ++k)
            if (byte >= f[k].offset && byte < f[k].offset + f[k].size)
                tag = f[k].tag;
        std::cout << tag << ' ';
        if (byte % 16 == 15 || byte + 1 == total) std::cout << '\n';
    }
    std::cout << "    legend: ";
    for (std::size_t k = 0; k < n; ++k)
        std::cout << f[k].tag << "=" << f[k].name
                  << "@" << f[k].offset << (k + 1 < n ? ", " : "");
    std::cout << ", .=padding\n";
}

int main() {
    std::cout << "=== 1. Natural alignment (x86-64) ===\n"
              << "  char:"    << alignof(char)     << "  short:"   << alignof(short)
              << "  int:"     << alignof(int)      << "  long long:" << alignof(long long)
              << "  double:"  << alignof(double)   << "  ptr:"    << alignof(void*)
              << "  max_align_t (malloc guarantee):" << alignof(std::max_align_t) << '\n';

    std::cout << "\n=== 2. Byte maps: find the holes ===\n";
    Field bad[]  = {{offsetof(Bad,c),  sizeof(char),'c',"c"}, {offsetof(Bad,i),  sizeof(int),  'i',"i"}, {offsetof(Bad,c2), sizeof(char),'C',"c2"}};
    Field good[] = {{offsetof(Good,i), sizeof(int),  'i',"i"}, {offsetof(Good,c), sizeof(char), 'c',"c"}, {offsetof(Good,c2),sizeof(char),'C',"c2"}};
    Field ugly[] = {{offsetof(Ugly,c), sizeof(char), 'c',"c"}, {offsetof(Ugly,d), sizeof(double),'d',"d"}, {offsetof(Ugly,c2),sizeof(char),'C',"c2"}};
    Field better[]= {{offsetof(Better,d),sizeof(double),'d',"d"}, {offsetof(Better,c), sizeof(char),'c',"c"}, {offsetof(Better,c2),sizeof(char),'C',"c2"}};
    print_layout("Bad  { char c; int i; char c2; }",  bad,  3, sizeof(Bad));
    print_layout("Good { int i; char c; char c2; }",  good, 3, sizeof(Good));
    print_layout("Ugly { char c; double d; char c2; }", ugly, 3, sizeof(Ugly));
    print_layout("Better{ double d; char c; char c2; }",better,3, sizeof(Better));

    std::cout << "\n=== 3. Tail padding exists because of arrays ===\n";
    Bad  ab[3];  Good ag[3];
    std::cout << "  Bad[3]:  &arr[1] - &arr[0] = " << &ab[1] - &ab[0]
              << " bytes (sizeof=" << sizeof(Bad)  << ") — stride must keep every i aligned\n"
              << "  Good[3]: &arr[1] - &arr[0] = " << &ag[1] - &ag[0]
              << " bytes (sizeof=" << sizeof(Good) << ")\n";

    std::cout << "\n=== 4. alignas(64): an int alone on a cache line ===\n";
    CacheLine* cl = new CacheLine{1};
    std::cout << "  sizeof = " << sizeof(CacheLine)
              << ", address = " << cl
              << ", address % 64 = " << (reinterpret_cast<std::uintptr_t>(cl) % 64)
              << "  <- 0 means cache-line aligned (C++17 aligned new honors it)\n";
    delete cl;

    std::cout << "\n=== 5. The scoreboard ===\n";
    std::cout << "  Bad:    6 B of data in " << sizeof(Bad)     << " B  -> "
              << 100 * (sizeof(Bad)     - 6) / sizeof(Bad)     << "% padding\n";
    std::cout << "  Good:   6 B of data in " << sizeof(Good)    << " B  -> "
              << 100 * (sizeof(Good)    - 6) / sizeof(Good)    << "% padding\n";
    std::cout << "  Ugly:  10 B of data in " << sizeof(Ugly)    << " B  -> "
              << 100 * (sizeof(Ugly)    - 10) / sizeof(Ugly)   << "% padding\n";
    std::cout << "  Better:10 B of data in " << sizeof(Better)  << " B  -> "
              << 100 * (sizeof(Better) - 10) / sizeof(Better)  << "% padding\n";
    return 0;
}