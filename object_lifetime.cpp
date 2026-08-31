// lesson_1_3_lifetimes.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_1_3_lifetimes.cpp -o lesson13 && ./lesson13
#include <iostream>
#include <string>

int g_depth = 0;                       // static storage (Lesson 1.1!) — indentation level

struct Tracer {
    const char* name;
    explicit Tracer(const char* n) : name(n) {          // ctor: object being BORN
        std::cout << std::string(g_depth * 2, ' ')
                  << "-> construct " << name << '\n';
        ++g_depth;
    }
    ~Tracer() {                                         // dtor: object DYING (still alive inside!)
        --g_depth;
        std::cout << std::string(g_depth * 2, ' ')
                  << "<- destroy  " << name << '\n';
    }
};

// ---- Law 6: callee locals die BEFORE control returns to caller ----
bool early_return(bool ret) {
    Tracer t("early_return local");
    if (ret) return true;               // Law 3: return still destroys t
    return false;
}

// ---- Law 5: members construct in DECLARATION order, not init-list order ----
struct Panel {
    Tracer header;                      // declared 1st  -> constructed 1st
    Tracer body;                        // declared 2nd  -> constructed 2nd
    Panel() : body("body (declared 2nd)"), header("header (declared 1st)") {}
    // init list deliberately written backwards — watch -Wreorder fire
};

// ---- frame addresses: one frame per call, each lower ----
void dive(int n) {
    int local = n;
    std::cout << "    frame depth " << n << ": &local = " << &local << '\n';
    if (n > 0) dive(n - 1);
}

int main() {
    std::cout << "=== Law 1+2+3: nesting, reverse destruction, early return ===\n";
    {
        Tracer outer("outer");
        {
            Tracer mid("mid");
            {
                Tracer inner("inner");
            }                                   // inner dies here
            std::cout << "  (inner block closed)\n";
        }                                       // mid dies here
        std::cout << "  (mid block closed)\n";
    }                                           // outer dies here

    std::cout << "\n=== Law 3: return exits through destructors ===\n";
    std::cout << "  calling early_return(true)...\n";
    bool ok = early_return(true);
    std::cout << "  back in main (callee object already dead), ok=" << ok << '\n';

    std::cout << "\n=== Law 4: loop body = fresh object per iteration ===\n";
    for (int i = 1; i <= 2; ++i) {
        Tracer t("loop iteration");
        if (i == 2) break;             // Law 3 again: break destroys too
    }

    std::cout << "\n=== Law 5: member order is DECLARATION order ===\n";
    {
        Panel p;                        // watch construction order vs init list!
    }

    std::cout << "\n=== frames: each recursive call sits LOWER ===\n";
    dive(3);

    std::cout << "\n=== anti-law: 'last use' is irrelevant ===\n";
    {
        Tracer t("never touched again");
        std::cout << "  t was last used above; it still dies at the brace ->\n";
    }
    return 0;
}