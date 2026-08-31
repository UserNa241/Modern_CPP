// lesson_1_7_bit_tools.cpp — the LEGAL way to touch bits
// build: g++ -std=c++23 -Wall -Wextra -Wpedantic lesson_1_7_bit_tools.cpp -o lesson17 && ./lesson17
#include <iostream>
#include <iomanip>
#include <bit>       // bit_cast, byteswap, endian
#include <utility>   // to_underlying
#include <cstring>   // memcpy
#include <cstdint>
#include <cstddef>

// ---- punning quarantined in one tiny named function (style rule) ----
constexpr std::uint32_t to_bits(float f)   { return std::bit_cast<std::uint32_t>(f); }
constexpr float         from_bits(std::uint32_t u) { return std::bit_cast<float>(u); }

struct Decoded { int sign; int exponent; std::uint32_t mantissa; };
constexpr Decoded decode_ieee754(std::uint32_t bits) {
    return {
        static_cast<int>((bits >> 31) & 1),              // top bit
        static_cast<int>((bits >> 23) & 0xFF) - 127,     // 8 exponent bits, bias 127
        bits & ((1u << 23) - 1)                          // low 23 bits
    };
}

int main() {
    std::cout << std::hex << std::setfill('0');

    std::cout << "=== 1. bit_cast: float -> its bits (constexpr!) ===\n";
    constexpr std::uint32_t pi  = to_bits(3.14f);
    constexpr auto dec_pi       = decode_ieee754(pi);          // decoded AT COMPILE TIME
    static_assert(dec_pi.sign == 0 && dec_pi.exponent == 1);
    std::cout << "  3.14f   = 0x" << std::setw(8) << pi
              << "  sign=" << dec_pi.sign << " exp=" << dec_pi.exponent
              << " mantissa=0x" << std::setw(6) << dec_pi.mantissa << '\n';

    constexpr std::uint32_t one  = to_bits(1.0f);
    constexpr auto dec_one       = decode_ieee754(one);
    static_assert(one == 0x3F800000u && dec_one.exponent == 0 && dec_one.mantissa == 0);
    std::cout << "  1.0f    = 0x" << std::setw(8) << one
              << "  sign=0 exp=0 mantissa=0  (the 'hidden 1' takes care of the rest)\n";

    std::cout << "\n=== 2. round trip ===\n";
    std::cout << std::dec;
    std::cout << "  from_bits(to_bits(2.718281828f)) = " << from_bits(to_bits(2.718281828f) << 0) << '\n';

    std::cout << "\n=== 3. memcpy agrees with bit_cast (and optimizes to nothing) ===\n";
    float f = 0.5f;
    std::uint32_t via_memcpy = 0;
    std::memcpy(&via_memcpy, &f, sizeof f);        // LEGAL: copies bytes, no fake access
    std::cout << std::hex
              << "  bit_cast: 0x" << std::setw(8) << to_bits(f)
              << "   memcpy: 0x" << std::setw(8) << via_memcpy
              << "   same? " << std::boolalpha << (to_bits(f) == via_memcpy) << '\n';

    std::cout << "\n=== 4. The universal exception: char/std::byte sees ANY object ===\n";
    std::uint16_t v = 0x1234;
    auto* bytes = reinterpret_cast<unsigned char*>(&v);   // always legal to LOOK at bytes
    std::cout << "  0x1234 in memory: " << std::setw(2) << static_cast<int>(bytes[0])
              << ' ' << std::setw(2) << static_cast<int>(bytes[1])
              << "  -> std::endian::native is "
              << (std::endian::native == std::endian::little ? "LITTLE" : "BIG") << '\n';

    std::cout << "\n=== 5. std::byteswap: network order games, no UB ===\n";
    std::cout << "  byteswap(0x1234) = 0x" << std::setw(4) << std::byteswap(v) << '\n';

    std::cout << "\n=== 6. std::to_underlying: enums without cast-spelling ===\n";
    enum class Level : std::uint8_t { debug = 0, info = 1, warn = 2 };
    std::cout << std::dec
              // NOTE: underlying type is uint8_t -> << would print it as a CHAR.
              // Cast to int so the NUMBER is printed. (Yes — this bit me 30 seconds ago.)
              << "  to_underlying(Level::warn) = "
              << static_cast<int>(std::to_underlying(Level::warn)) << '\n';
    return 0;
}
