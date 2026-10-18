#pragma once
#include "rendering/geometry/terrain/TerrainTopology.h"
#include <bit>
#include <limits>
#include <optional>
#include <stdexcept>

namespace rendering {
enum class TerrainMorphStorage { Transient, Retained };
struct TerrainMorphFootprint {
    glm::dvec3 eyeRadial{0};
    double metersPerRadius=0,radiusMeters=0,transitionMeters=0;
    bool operator==(const TerrainMorphFootprint&) const = default;
};
// CPU-owned immutable counterpart of the retained GPU source. Keep integer
// words: double low words may spell a NaN if transported through float values.
// Head: radial double3 + fine float3; tail: parent double3 + original float3.
struct TerrainMorphData {
    TerrainGenerationKey generation;
    double metersPerRadius=0;
    std::optional<TerrainMorphFootprint> phase; // Baked output pose, not target identity.
    std::vector<std::uint32_t> targets,corners;
    std::size_t samples() const {return targets.size()/18;}
    std::uint64_t bytes() const {return (targets.size()+corners.size())*sizeof(std::uint32_t);}
    glm::dvec3 radial(std::size_t sample) const {return double3(head(sample));}
    glm::vec3 fine(std::size_t sample) const {return float3(head(sample)+6);}
    glm::dvec3 parent(std::size_t sample) const {return double3(9*samples()+head(sample));}
    glm::vec3 original(std::size_t sample) const {return float3(9*samples()+head(sample)+6);}
private:
    std::size_t head(std::size_t sample) const {
        if(sample>=samples()) throw std::out_of_range("Terrain morph sample outside retained source");
        return 9*sample;
    }
    glm::dvec3 double3(std::size_t offset) const {
        glm::dvec3 value;
        for(int j=0;j<3;++j) value[j]=std::bit_cast<double>(std::uint64_t(targets.at(offset+2*j)) |
            (std::uint64_t(targets.at(offset+2*j+1))<<32));
        return value;
    }
    glm::vec3 float3(std::size_t offset) const {
        return {std::bit_cast<float>(targets.at(offset)),std::bit_cast<float>(targets.at(offset+1)),
            std::bit_cast<float>(targets.at(offset+2))};
    }
};
inline void validateTerrainMorphFootprint(const TerrainMorphFootprint& f) {
    const auto& eye=f.eyeRadial;
    if(!std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z) ||
       std::abs(glm::dot(eye,eye)-1)>1e-12 || !std::isfinite(f.metersPerRadius) || f.metersPerRadius<=0 ||
       !std::isfinite(f.radiusMeters) || f.radiusMeters<0 || !std::isfinite(f.transitionMeters) || f.transitionMeters<=0)
        throw std::invalid_argument("Invalid terrain morph footprint");
}
inline double terrainMorphWeight(const glm::dvec3& radial,const TerrainMorphFootprint& f) {
    const double blend=std::clamp((glm::length(radial-f.eyeRadial)*f.metersPerRadius-f.radiusMeters)/f.transitionMeters,0.,1.);
    return blend*blend*(3-2*blend);
}
// Frame-local state is separate from immutable topology/generation identity.
// Context/simulation-thread use only; worker snapshots never mutate this state.
class TerrainMorphState {
public:
    const TerrainMorphFootprint* footprint() const {return footprint_?&*footprint_:nullptr;}
    std::uint64_t revision() const {return revision_;}
    void set(const TerrainMorphFootprint& f) {
        validateTerrainMorphFootprint(f);
        if(footprint_ && *footprint_==f) return;
        if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Terrain morph revision exhausted");
        footprint_=f;++revision_;
    }
private:
    std::optional<TerrainMorphFootprint> footprint_;
    std::uint64_t revision_=0;
};
// Match the render contract's two float conversions before radial projection.
inline glm::dvec3 terrainPosition(const PlanetField& field,const TerrainInputSample& s,
                                  const TerrainSurfacePolicy& policy) {
    const glm::dvec3 r(s.radial[0],s.radial[1],s.radial[2]);
    const auto p=r*(1+field.heightAt(r,policy)/field.radiusWorld());
    volatile float rounded[3];
    for(int j=0;j<3;++j) rounded[j]=static_cast<float>(p[j]);
    const glm::dvec3 converted(rounded[0],rounded[1],rounded[2]);
    return glm::vec3(converted-glm::normalize(converted)*(s.sinkMeters/(field.radiusWorld()*field.metersPerUnit())));
}
inline glm::dvec3 terrainParentPosition(const PlanetField& field,const TerrainInputSample& s,
    const TerrainMorphSample& morph,const TerrainSurfacePolicy& parentPolicy) {
    const auto a=terrainPosition(field,morph.parent[0],parentPolicy);
    if(morph.parent[0]==morph.parent[1] && morph.parent[0]==morph.parent[2])
        return a;
    const auto b=terrainPosition(field,morph.parent[1],parentPolicy);
    const auto c=terrainPosition(field,morph.parent[2],parentPolicy);
    const auto normal=glm::cross(b-a,c-a);
    const glm::dvec3 r(s.radial[0],s.radial[1],s.radial[2]);
    return glm::dot(normal,normal)==0 ? a : r*(glm::dot(normal,a)/glm::dot(normal,r));
}
inline glm::vec3 terrainMorphPosition(const PlanetField& field,const TerrainInputSample& s,
    const TerrainSurfacePolicy& policy,const TerrainMorphSample& morph,const TerrainSurfacePolicy& parentPolicy,double weight) {
    const auto fine=terrainPosition(field,s,policy);
    if(weight==0) return fine;
    const auto parent=terrainParentPosition(field,s,morph,parentPolicy);
    return glm::vec3(fine+(parent-fine)*weight);
}
inline glm::vec3 terrainMorphPosition(const PlanetField& field,const TerrainInputSample& s,
    const TerrainSurfacePolicy& policy,const TerrainMorphSample& morph,const TerrainSurfacePolicy& parentPolicy) {
    return terrainMorphPosition(field,s,policy,morph,parentPolicy,morph.blend[0]);
}
}
