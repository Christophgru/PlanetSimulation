#include "rendering/geometry/terrain/TerrainQueryCache.h"
namespace rendering {
double TerrainQueryCache::heightAt(const glm::dvec3& radial) {
    ++stats_.requests;
    // A replaced field value must never reuse heights from its former parameters.
    if (fieldFingerprint_!=field_.fingerprint()) {
        values_.clear();fieldFingerprint_=field_.fingerprint();
    }
    const Key key{std::bit_cast<std::uint64_t>(radial.x),std::bit_cast<std::uint64_t>(radial.y),
                  std::bit_cast<std::uint64_t>(radial.z)};
    if (const auto it=values_.find(key);it!=values_.end()) { ++stats_.hits; return it->second; }
    // Validate and evaluate before caching; NaN/zero queries never enter it.
    const double height=field_.heightAt(radial,policy_);
    ++stats_.evaluations;
    if (capacity_) {
        // Deterministic bounded batches avoid an unbounded whole-planet mirror.
        if (values_.size()==capacity_) values_.clear();
        values_.emplace(key,height);
    }
    return height;
}
}
