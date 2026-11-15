// lesson_1_6_temporaries.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_1_6_temporaries.cpp -o lesson16 && ./lesson16
#include <iostream>
#include <string>

int g_depth = 0;
struct Tracer {
    const char* name;
    explicit Tracer(const char* n) : name(n) {
        std::cout << std::string(g_depth*2,' ') << "+ " << name << '\n'; ++g_depth;
    }
    ~Tracer() { --g_depth; std::cout << std::string(g_depth*2,' ') << "- " << name << '\n'; }
    void touch() const { std::cout << "      (still alive: " << name << ")\n"; }
};

Tracer make(const char* n) { return Tracer{n}; }
void use(const Tracer& t) { std::cout << "      [use] " << t.name << '\n'; }
void use2(const Tracer& a, const Tracer& b) {
    std::cout << "      [use2] " << a.name << " and " << b.name << '\n';
}
const Tracer& identity(const Tracer& t) { return t; }   // returns its parameter BY REFERENCE
struct Gift {                       // contains a Tracer
    Tracer t;
    explicit Gift(const char* n) : t{n} {}
};

int main() {
    std::cout << "=== 1. Full-expression lifetime (Law T1) ===\n";
    std::cout << "  before\n";
    use(make("A"));                    // temporary lives until the ';' of THIS line
    std::cout << "  after   <- 'A' died BETWEEN 'use' returning and this line\n";

    std::cout << "\n=== 2. Two temporaries die in REVERSE construction order ===\n";
    use2(make("X"), make("Y"));        // note: argument evaluation order is UNSPECIFIED
    std::cout << "  (last constructed = first destroyed)\n";

    std::cout << "\n=== 3. THE MAGIC: direct binding extends (Law T2) ===\n";
    {
        const Tracer& r = make("ext-const&");
        std::cout << "  the full expression is OVER, yet:\n";
        r.touch();
        Tracer&& rr = make("ext-&&");
        std::cout << "  rvalue references extend too:\n";
        rr.touch();
        std::cout << "  leaving scope -> both die NOW, with their references:\n";
    }

    std::cout << "\n=== 4. Extension covers the WHOLE temporary (member access) ===\n";
    {
        const Tracer& rt = Gift{"gift-box"}.t;   // binds to a MEMBER of a prvalue (Gift has a ctor)
        std::cout << "  entire Gift temporary kept alive:\n";
        rt.touch();
        std::cout << "  leaving scope:\n";
    }

    std::cout << "\n=== 5. TRAP 1: extension does NOT pass through a function ===\n";
    {
        [[maybe_unused]] const Tracer& r2 = identity(make("passed"));
        std::cout << "  full expression ended -> see '- passed' above? ALREADY DEAD.\n"
                  << "  r2 is a dangling reference now (initializer was an LVALUE,\n"
                  << "  because identity returns T&) — we must NOT touch it.\n";
    }
    return 0;
}
