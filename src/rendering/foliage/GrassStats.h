#pragma once
#include <cstddef>
namespace rendering {
struct GrassPreparationStats {
    double placementMs = 0, sortMs = 0, uploadMs = 0;
    unsigned rebuilds = 0;
    std::size_t uploadedBytes = 0;
};
}
