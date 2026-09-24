#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

namespace rendering {

inline std::uint32_t starCellHash(const std::array<std::int32_t, 3>& cell,
                                  std::uint32_t seed) {
    std::uint32_t hash = seed ^ 0x9e3779b9u;
    hash ^= static_cast<std::uint32_t>(cell[0]) * 0x85ebca6bu;
    hash ^= static_cast<std::uint32_t>(cell[1]) * 0xc2b2ae35u;
    hash ^= static_cast<std::uint32_t>(cell[2]) * 0x27d4eb2fu;
    hash ^= hash >> 16u;
    hash *= 0x7feb352du;
    hash ^= hash >> 15u;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16u;
    return hash;
}

inline double starHashUnit(std::uint32_t hash) {
    return static_cast<double>(hash >> 8u) / 16777215.0;
}

inline bool starCellVisible(const std::array<std::int32_t, 3>& cell,
                            std::uint32_t seed, double density) {
    if (density < 0.0 || density > 1.0)
        throw std::invalid_argument("Star density must be between zero and one");
    return density > 0.0 && starHashUnit(starCellHash(cell, seed)) >= 1.0 - density;
}

} // namespace rendering
