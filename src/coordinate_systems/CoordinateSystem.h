#pragma once

#include "../math/Vector3.h"

enum class CoordinateSpace {
    SOLAR_SYSTEM,   // Global universe scale (double)
    PLANET_LOCAL    // Planet-relative (float for GPU)
};

class CoordinateSystem {
public:
    virtual ~CoordinateSystem() = default;
    
    virtual CoordinateSpace space() const = 0;
    virtual Vector3 origin() const = 0;
    virtual Vector3 toLocal(const Vector3& global) const = 0;
    virtual Vector3 toGlobal(const Vector3& local) const = 0;
};
