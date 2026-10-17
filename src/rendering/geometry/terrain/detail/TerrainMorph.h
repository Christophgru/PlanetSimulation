#pragma once
#include "rendering/geometry/terrain/TerrainTopology.h"

namespace rendering {
struct TerrainMorphFootprint {
    glm::dvec3 eyeRadial{0};
    double metersPerRadius=0,radiusMeters=0,transitionMeters=0;
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
inline glm::vec3 terrainMorphPosition(const PlanetField& field,const TerrainInputSample& s,
    const TerrainSurfacePolicy& policy,const TerrainMorphSample& morph,const TerrainSurfacePolicy& parentPolicy) {
    const auto fine=terrainPosition(field,s,policy);
    if(morph.blend[0]==0) return fine;
    const auto a=terrainPosition(field,morph.parent[0],parentPolicy);
    if(morph.parent[0]==morph.parent[1] && morph.parent[0]==morph.parent[2])
        return glm::vec3(fine+(a-fine)*morph.blend[0]);
    const auto b=terrainPosition(field,morph.parent[1],parentPolicy);
    const auto c=terrainPosition(field,morph.parent[2],parentPolicy);
    const auto normal=glm::cross(b-a,c-a);
    const glm::dvec3 r(s.radial[0],s.radial[1],s.radial[2]);
    const auto parent=glm::dot(normal,normal)==0 ? a : r*(glm::dot(normal,a)/glm::dot(normal,r));
    return glm::vec3(fine+(parent-fine)*morph.blend[0]);
}
}
