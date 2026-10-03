#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <unordered_map>
#include "rendering/geometry/terrain/PlanetField.h"
namespace rendering {
struct TerrainQueryStats { std::uint64_t requests=0,evaluations=0,hits=0; };
// Build-local bounded cache. Exact input bits preserve the legacy normalization
// order; no rounded coordinate keys and no shared mutable state between workers.
class TerrainQueryCache {
public:
    explicit TerrainQueryCache(const PlanetField& field,std::size_t capacity=8192)
        : field_(field),capacity_(capacity),fieldFingerprint_(field.fingerprint()) {}
    double heightAt(const glm::dvec3& radial);
    const auto& stats() const { return stats_; }
    std::size_t size() const { return values_.size(); }
    const PlanetField& field() const { return field_; }
private:
    using Key=std::array<std::uint64_t,3>;
    struct Hash { std::size_t operator()(const Key& key) const {
        return key[0] ^ std::rotl(key[1],21) ^ std::rotl(key[2],42);
    }};
    const PlanetField& field_;
    std::size_t capacity_;
    std::uint64_t fieldFingerprint_;
    TerrainQueryStats stats_{};
    std::unordered_map<Key,double,Hash> values_;
};
}
