#include "rendering/geometry/terrain/TerrainTopology.h"
#include <bit>
#include <cmath>
#include <map>
#include <stdexcept>
namespace rendering {
std::uint64_t terrainHashWord(std::uint64_t hash,std::uint64_t word,int bytes) {
    for(int i=0;i<bytes;++i) { hash^=(word>>(8*i))&255;hash*=1099511628211ull; }
    return hash;
}
void TerrainTopology::validate() const {
    if(indices.size()%3 || generation.fieldVersion!=PlanetField::version || generation.topologyVersion!=version() ||
       generation.backend!=TerrainBackend::Cpu) throw std::invalid_argument("Invalid terrain topology contract");
    for(const auto* p:{&surfacePolicy,&parentPolicy}) {
        if(p==&parentPolicy && morph.empty()) continue;
        const auto& policy=*p;
        if(policy.enabled()) {
            for(const auto& values:{policy.eyeEdge,policy.distances})
                for(double v:values) if(!std::isfinite(v)) throw std::invalid_argument("Invalid terrain surface policy");
            const auto& d=policy.distances;
            const glm::dvec3 eye(policy.eyeEdge[0],policy.eyeEdge[1],policy.eyeEdge[2]);
            if(d[0]<=0 || d[1]<=d[0] || d[2]<=0 || d[3]<0 || policy.eyeEdge[3]<=0 ||
               (glm::dot(eye,eye)!=0 && std::abs(glm::dot(eye,eye)-1)>1e-12))
                throw std::invalid_argument("Invalid terrain surface policy");
            for(int i=0;i<8;++i) if(!std::isfinite(policy.spacing[i]) || policy.spacing[i]<0 ||
               (i && policy.spacing[i]>policy.spacing[i-1]))
                throw std::invalid_argument("Invalid terrain parent spacing");
        } else if(policy!=TerrainSurfacePolicy{}) throw std::invalid_argument("Invalid disabled terrain surface policy");
    }
    if(!morph.empty()) {
        if(morph.size()!=samples.size() || !parentPolicy.enabled()) throw std::invalid_argument("Invalid terrain morph contract");
        for(const auto& m:morph) {
            if(m.blend[1]!=0 || m.blend[2]!=0 || m.blend[3]!=0) throw std::invalid_argument("Invalid reserved terrain morph data");
            if(!std::isfinite(m.blend[0]) || m.blend[0]<0 || m.blend[0]>1) throw std::invalid_argument("Invalid terrain morph weight");
            for(const auto& p:m.parent) {
                double length2=0;
                for(double x:p.radial) {if(!std::isfinite(x)) throw std::invalid_argument("Invalid terrain parent");length2+=x*x;}
                if(std::abs(length2-1)>1e-12 || !std::isfinite(p.sinkMeters) || p.sinkMeters<0)
                    throw std::invalid_argument("Invalid terrain parent");
            }
        }
    }
    for(const auto& s:samples) {
        double length2=0;
        for(double x:s.radial) {if(!std::isfinite(x)) throw std::invalid_argument("Nonfinite terrain radial");length2+=x*x;}
        if(std::abs(length2-1)>1e-12 || !std::isfinite(s.sinkMeters) || s.sinkMeters<0)
            throw std::invalid_argument("Invalid terrain sample direction or sink");
    }
    for(auto i:indices) if(i>=samples.size()) throw std::invalid_argument("Invalid terrain topology index");
    if (generation.topology!=contentFingerprint())
        throw std::invalid_argument("Stale terrain topology generation key");
}
std::uint64_t TerrainTopology::contentFingerprint() const {
    auto hash=14695981039346656037ull;
    hash=terrainHashWord(hash,samples.size());
    hash=terrainHashWord(hash,indices.size());
    for(const auto& s:samples) {
        for(double x:s.radial) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(x));
        hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(s.sinkMeters));
    }
    if(surfacePolicy.enabled()) {
        hash=terrainHashWord(hash,surfacePolicy.version());
        for(double x:surfacePolicy.eyeEdge) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(x));
        for(double x:surfacePolicy.distances) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(x));
        for(double x:surfacePolicy.spacing) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(x));
    }
    if(!morph.empty()) {
        hash=terrainHashWord(hash,4);
        for(double x:parentPolicy.eyeEdge) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(x));
        for(double x:parentPolicy.distances) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(x));
        for(double x:parentPolicy.spacing) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(x));
        for(const auto& m:morph) {
            for(const auto& p:m.parent) {
                for(double x:p.radial) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(x));
                hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(p.sinkMeters));
            }
            for(double x:m.blend) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(x));
        }
    }
    for(auto i:indices) hash=terrainHashWord(hash,i,4);
    return hash;
}
void TerrainTopology::canonicalize(std::uint64_t fieldFingerprint) {
    std::map<std::array<std::uint64_t,4>,std::uint32_t> ids;
    std::vector<TerrainInputSample> unique;
    std::vector<TerrainMorphSample> uniqueMorph;
    if(!morph.empty()) {
        // Streaming starts from canonical parent IDs; edge-owned midpoints are
        // unique already. Compact unused probes by ID instead of rebuilding a
        // whole-shell ordered map for each sub-metre camera movement.
        std::vector<std::uint32_t> compact(samples.size(),~std::uint32_t(0));
        for(auto& i:indices) {
            auto& next=compact.at(i);
            if(next==~std::uint32_t(0)) {
                next=unique.size();unique.push_back(samples.at(i));uniqueMorph.push_back(morph.at(i));
            }
            i=next;
        }
    } else {
    for(auto& i:indices) {
        const auto& s=samples.at(i);
        const std::array<std::uint64_t,4> key{std::bit_cast<std::uint64_t>(s.radial[0]),
            std::bit_cast<std::uint64_t>(s.radial[1]),std::bit_cast<std::uint64_t>(s.radial[2]),
            std::bit_cast<std::uint64_t>(s.sinkMeters)};
        auto [it,inserted]=ids.try_emplace(key,static_cast<std::uint32_t>(unique.size()));
        if(inserted) {
            unique.push_back(s);
            if(!morph.empty()) uniqueMorph.push_back(morph.at(i));
        }
        i=it->second;
    }
    }
    samples=std::move(unique);morph=std::move(uniqueMorph);
    std::vector<glm::vec3>().swap(planningPositions);
    generation.topologyVersion=version();
    generation.field=fieldFingerprint;
    generation.topology=contentFingerprint();
    uniqueSamples=samples.size();topologyInputBytes=morph.size()*sizeof(TerrainMorphSample)+(morph.empty()?0:sizeof(parentPolicy))+samples.size()*sizeof(TerrainInputSample)+indices.size()*sizeof(std::uint32_t)+ (surfacePolicy.enabled()?sizeof(surfacePolicy):0);
    validate();
}
}
