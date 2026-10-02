#pragma once
#include <glm/glm.hpp>
#include <optional>
#include <vector>
#include <cstdint>

namespace rendering {
struct TrailSegment { glm::dvec3 start, end; };
// Persistent body-local metre coordinates. A bounded history prevents walking
// indefinitely from growing CPU/GPU storage; only the oldest marks are evicted.
class GrassTrail {
public:
    static constexpr std::size_t capacity=2048;
    static constexpr double radius=.6;
    void observe(const glm::dvec3& groundPosition,bool grounded);
    void breakPath() { anchor_.reset(); }
    void restore(const std::vector<TrailSegment>& segments);
    const std::vector<TrailSegment>& segments() const { return segments_; }
    std::uint64_t revision() const { return revision_; }
    // Four RGBA texels per stackless BVH node: bounds/escape, bounds/leaf,
    // segment start and end. Positions are relative to origin, in metres.
    std::vector<glm::vec4> hierarchy(const glm::dvec3& origin) const;
private:
    std::vector<TrailSegment> segments_;
    std::optional<glm::dvec3> anchor_;
    std::uint64_t revision_=0;
};
}
