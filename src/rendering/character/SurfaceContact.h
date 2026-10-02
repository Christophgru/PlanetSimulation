#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include <glm/glm.hpp>

namespace rendering {
struct GroundContact {
    glm::dvec3 position{0}; // Planet-local metres, including the planet radius.
    glm::dvec3 normal{0,1,0};
};
using GroundQuery = std::function<GroundContact(const glm::dvec3&)>;

// Radial collision with the selected, rendered terrain, including LOD sinking.
// A small triangle cache makes resting feet and short camera probes inexpensive.
class SurfaceContact {
public:
    void bind(const std::vector<float>& vertices, const std::vector<unsigned>& indices,
              std::uint64_t revision, double radiusMeters, int stride = 9);
    GroundContact sample(const glm::dvec3& direction, const GroundQuery& fallback);
    void clear();
private:
    bool hit(unsigned triangle, const glm::dvec3& radial, GroundContact& result) const;
    const std::vector<float>* vertices_ = nullptr;
    const std::vector<unsigned>* indices_ = nullptr;
    std::uint64_t revision_ = 0;
    double radiusMeters_ = 1;
    int stride_ = 9;
    std::array<unsigned,16> cache_{};
    unsigned cached_ = 0, next_ = 0;
};
}
