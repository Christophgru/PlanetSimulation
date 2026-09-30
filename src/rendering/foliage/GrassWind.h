#pragma once
#include <cmath>

namespace rendering {
// grass.vert uses 256-cell periodic noise and binary-fraction advection rates.
// Their common period allows a bounded float phase without a time-wrap jump.
inline constexpr double grassWindPeriodSeconds = 8192.0;
inline float grassWindTime(double seconds) {
    return static_cast<float>(std::remainder(seconds, grassWindPeriodSeconds));
}
}
