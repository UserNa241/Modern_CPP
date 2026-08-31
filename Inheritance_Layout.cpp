// lesson_1_4_inheritance_layout.cpp
// build: g++ -std=c++20 -Wall -Wextra -Wpedantic lesson_1_4_inheritance_layout.cpp -o lesson14 && ./lesson14
#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>   // memcpy — legal byte inspection
#include <cstddef>   // ptrdiff_t

// ---------------- helpers ----------------
int g_depth = 0;
struct Tracer {
    const char* name;
    explicit Tracer(const char* n) : name(n) {
        std::cout << std::string(g_depth*2,' ') << "+ " << name << '\n'; ++g_depth;
    }
    ~Tracer() { --g_depth; std::cout << std::string(g_depth*2,' ') << "- " << name << '\n'; }
};

template <class Owner, class Member>
std::ptrdiff_t byte_offset(const Owner* o, const Member& m) {   // offsetof replacement
    return reinterpret_cast<const char*>(&m) - reinterpret_cast<const char*>(o);
}

std::uintptr_t first8(const void* p) {           // peek at the hidden first 8 bytes
    std::uintptr_t v = 0;
    std::memcpy(&v, p, sizeof v);
    return v;
}

// ---------------- 1. embedding ----------------
struct Plain        { int a; };
struct PlainDerived : Plain { int b; };

// ---------------- 2. vptr ----------------
struct NoVirtual { int a, b; };
struct HasVirtual {
    int a, b;
    virtual void speak() { std::cout << "HasVirtual::speak\n"; }
    virtual ~HasVirtual() = default;
};

// ---------------- 3. ctor order + trap ----------------
struct Base3 {
    Tracer m{"member of Base"};
    Base3() { std::cout << "        Base body\n"; }
};
struct Derived3 : Base3 {
    Tracer m{"member of Derived"};
    Derived3() { std::cout << "        Derived body\n"; }
};
struct Speaker {
    virtual void name() const { std::cout << "Speaker"; }
    Speaker() { std::cout << "        inside Speaker ctor, I am a -> "; name(); std::cout << '\n'; }
};
struct Phone : Speaker {
    void name() const override { std::cout << "Phone"; }
};

// ---------------- 5. multiple inheritance ----------------
struct A5 { int a; };
struct B5 { int b; };
struct AB : A5, B5 { int c; };

// ---------------- 6. dynamic_cast ----------------
struct P1 { virtual ~P1() = default; };
struct P2 { virtual ~P2() = default; };
struct Both : P1, P2 {};

// ---------------- 7. EBO ----------------
struct Empty {};
struct HoldsEmpty { Empty e; int x; };
struct DerivesEmpty : Empty { int x; };

// ---------------- 8. diamond ----------------
struct VBase { int v; };
struct L : VBase   { int l; };
struct R : VBase   { int r; };
struct Diamond : L, R { int d; };
struct vL : virtual VBase { int l; };
struct vR : virtual VBase { int r; };
struct VDiamond : vL, vR  { int d; };

int main() {
    std::cout << "=== 1. Derived EMBEDS Base (no virtuals) ===\n";
    std::cout << "  sizeof(Plain)=" << sizeof(Plain)
              << "  sizeof(PlainDerived)=" << sizeof(PlainDerived) << '\n';
    PlainDerived pd{1, 2};
    std::cout << "  a at offset " << byte_offset(&pd, pd.a)
              << "  (Base subobject), b at offset " << byte_offset(&pd, pd.b) << '\n';
    std::cout << "  &pd == (Plain*)&pd ? " << (&pd == static_cast<Plain*>(&pd))
              << "   <- upcast: same address, zero cost\n";

    std::cout << "\n=== 2. The hidden vptr ===\n";
    std::cout << "  sizeof(NoVirtual)=" << sizeof(NoVirtual)
              << "  sizeof(HasVirtual)=" << sizeof(HasVirtual)
              << "   (+8 bytes for one virtual!)\n";
    HasVirtual h1{}, h2{};
    std::cout << "  a at offset " << byte_offset(&h1, h1.a)
              << " -> bytes 0..7 are taken by something hidden:\n"
              << "  first8(h1) = 0x" << std::hex << first8(&h1) << '\n'
              << "  first8(h2) = 0x" << first8(&h2) << std::dec
              << "   SAME -> both objects point at ONE shared vtable\n";
    Both bobj{};
    std::cout << "  first8(Both object) = 0x" << std::hex << first8(&bobj)
              << std::dec << "   DIFFERENT class -> different vtable\n";

    std::cout << "\n=== 3. Construction order + ctor-virtual trap ===\n";
    { Derived3 d; (void)d; }
    std::cout << "  ---\n";
    Phone ph;
    std::cout << "        after construction: "; ph.name(); std::cout << '\n';

    std::cout << "\n=== 5. Multiple inheritance: one object, TWO addresses ===\n";
    std::cout << "  sizeof(AB)=" << sizeof(AB) << "   [a@0][b@4][c@8]\n";
    AB ab{1,2,3};
    A5* pa = &ab;
    B5* pb = &ab;
    std::cout << "  &ab      = " << &ab << '\n'
              << "  (A5*)&ab = " << pa << "   == &ab (base #1 at offset 0)\n"
              << "  (B5*)&ab = " << pb << "   offset +"
              << reinterpret_cast<std::uintptr_t>(pb) - reinterpret_cast<std::uintptr_t>(&ab)
              << " inside the same object!\n"
              << "  static_cast<AB*>(pb) adjusts back: "
              << (static_cast<AB*>(pb) == &ab) << '\n';

    std::cout << "\n=== 6. dynamic_cast: checked casts at runtime ===\n";
    Both* bp = &bobj;
    P1* p1 = bp;
    P2* p2 = bp;
    std::cout << "  P1*=" << static_cast<void*>(p1) << "  P2*=" << static_cast<void*>(p2)
              << "  (P2 subobject at +8)\n"
              << "  downcast from second base: dynamic_cast<Both*>(p2) == &both? "
              << (dynamic_cast<Both*>(p2) == bp) << '\n'
              << "  cross-cast: dynamic_cast<P2*>(p1) == p2? "
              << (dynamic_cast<P2*>(p1) == p2) << '\n';
    P1 solo{};
    std::cout << "  wrong downcast: dynamic_cast<Both*>(&solo) = "
              << dynamic_cast<Both*>(&solo) << "  (nullptr = checked & refused)\n";

    std::cout << "\n=== 7. Empty base optimization ===\n";
    std::cout << "  sizeof(Empty)=" << sizeof(Empty)
              << "  sizeof(HoldsEmpty)=" << sizeof(HoldsEmpty)
              << "  sizeof(DerivesEmpty)=" << sizeof(DerivesEmpty)
              << "   <- as a BASE, Empty costs 0\n";

    std::cout << "\n=== 8. The diamond: duplicated vs shared base ===\n";
    std::cout << "  non-virtual: sizeof(Diamond)=" << sizeof(Diamond)
              << "  (VBase stored TWICE)\n";
    Diamond dia{};
    VBase* vl = static_cast<L*>(&dia);
    VBase* vr = static_cast<R*>(&dia);
    std::cout << "    VBase* via L: " << static_cast<void*>(vl)
              << "   via R: " << static_cast<void*>(vr)
              << "   same? " << (vl == vr) << " (two subobjects!)\n";
    std::cout << "  virtual:     sizeof(VDiamond)=" << sizeof(VDiamond)
              << "  (bigger: hidden base-locator bookkeeping)\n";
    VDiamond vd{};
    VBase* wl = static_cast<vL*>(&vd);
    VBase* wr = static_cast<vR*>(&vd);
    std::cout << "    VBase* via vL: " << static_cast<void*>(wl)
              << "   via vR: " << static_cast<void*>(wr)
              << "   same? " << (wl == wr) << " (ONE shared subobject)\n";
	system("pause");
    return 0;
}
