#pragma once

#include <bit>
#include <cstdint>

// ¿Es un número (no NaN ni infinito)? Por bits: con -ffast-math, std::isfinite se optimiza a
// "siempre verdadero".
inline bool finiteNumber(double v) {
    return (std::bit_cast<std::uint64_t>(v) & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
}
