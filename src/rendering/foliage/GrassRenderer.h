#pragma once
#include <cstddef>
#include "rendering/foliage/procedural/ProceduralGrass.h"
class Mesh;
namespace config { struct PlanetConfig; }

namespace rendering {
struct GrassDrawStats {
    std::size_t blades=0, vertices=0, triangles=0, batches=0, instanceBytes=0;
};
class GrassRenderer {
    PublicationProfiler* trace_=nullptr;
    const std::uint64_t* epoch_=nullptr;
    const std::vector<std::uint64_t>* serials_=nullptr;
    const std::vector<int>* masks_=nullptr;
    const std::vector<Mesh>* water_=nullptr;
public:
    // Upload triangle IDs; placement and wind stay on the GPU.
    ProceduralGrass procedural;
    Shader& shader=procedural.shader;
    GrassRenderer() = default;
    GrassRenderer(const GrassRenderer&) = delete;
    GrassRenderer& operator=(const GrassRenderer&) = delete;
    ~GrassRenderer()=default;
    void clear();
    // Borrow stable renderer members, never topology snapshots or vector data.
    // Standalone/off-live owners remain unbound; reload children own that work.
    void diagnostics(PublicationProfiler* trace,const std::uint64_t* epoch,
        const std::vector<std::uint64_t>* serials,const std::vector<int>* masks,const std::vector<Mesh>* water) {
        if(!trace || !trace->enabled()) return;
        trace_=trace;epoch_=epoch;serials_=serials;masks_=masks;water_=water;
    }
    GrassPreparationStats prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
                 double metersPerWorldUnit,const glm::dvec3& eyeBody,std::uint64_t otherTerrainBytes=0);
    std::size_t count(std::size_t index) const;
    GrassDrawStats drawStats(std::size_t index) const;
    void draw(std::size_t index,const GrassPass* pass=nullptr) const;
};
} // namespace rendering
