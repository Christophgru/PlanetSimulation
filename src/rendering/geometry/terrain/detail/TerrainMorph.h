#pragma once
#include "rendering/geometry/terrain/TerrainTopology.h"
#include <limits>
#include <optional>
#include <stdexcept>

namespace rendering {
struct TerrainMorphFootprint {
    glm::dvec3 eyeRadial{0};
    double metersPerRadius=0,radiusMeters=0,transitionMeters=0;
    bool operator==(const TerrainMorphFootprint&) const = default;
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
