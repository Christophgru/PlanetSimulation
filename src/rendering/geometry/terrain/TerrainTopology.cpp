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
    if(indices.size()%3 || generation.fieldVersion!=PlanetField::version || generation.topologyVersion!=1 ||
       generation.backend!=TerrainBackend::Cpu) throw std::invalid_argument("Invalid terrain topology contract");
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
    for(auto i:indices) hash=terrainHashWord(hash,i,4);
    return hash;
}
void TerrainTopology::canonicalize(std::uint64_t fieldFingerprint) {
    std::map<std::array<std::uint64_t,4>,std::uint32_t> ids;
    std::vector<TerrainInputSample> unique;
    for(auto& i:indices) {
        const auto& s=samples.at(i);
        const std::array<std::uint64_t,4> key{std::bit_cast<std::uint64_t>(s.radial[0]),
            std::bit_cast<std::uint64_t>(s.radial[1]),std::bit_cast<std::uint64_t>(s.radial[2]),
            std::bit_cast<std::uint64_t>(s.sinkMeters)};
        auto [it,inserted]=ids.try_emplace(key,static_cast<std::uint32_t>(unique.size()));
        if(inserted) unique.push_back(s);
        i=it->second;
    }
    samples=std::move(unique);
    std::vector<glm::vec3>().swap(planningPositions);
    generation.field=fieldFingerprint;
    generation.topology=contentFingerprint();
    uniqueSamples=samples.size();topologyInputBytes=samples.size()*sizeof(TerrainInputSample)+indices.size()*sizeof(std::uint32_t);
    validate();
}
}
