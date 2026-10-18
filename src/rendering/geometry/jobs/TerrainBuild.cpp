#include "rendering/geometry/jobs/TerrainBuild.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace rendering {
void TerrainBuildRequest::validate() const {
    const auto& k=identity;
    if(!k.epoch || !k.serial || k.bodyName.empty() || k.bodyName!=planet.name ||
       k.field!=surface.field().fingerprint() || k.fieldVersion!=PlanetField::version || k.topologyVersion!=surface.topologyVersion() ||
       (k.backend!=TerrainBackend::Cpu && k.backend!=TerrainBackend::Compute) ||
       (k.resident && k.backend!=TerrainBackend::Compute) || (k.localMask!=0 && k.localMask!=1) ||
       !std::isfinite(k.eye.x) || !std::isfinite(k.eye.y) || !std::isfinite(k.eye.z) || glm::length(k.eye)<=0 ||
       !std::isfinite(glm::length(k.eye)) ||
       !std::isfinite(metersPerUnit) || metersPerUnit<=0 ||
       metersPerUnit!=surface.field().metersPerUnit() || planet.radius!=surface.field().radiusWorld())
        throw std::invalid_argument("Invalid terrain worker snapshot");
    if(preparedPhase) {
        validateTerrainMorphFootprint(*preparedPhase);
        const auto& f=*preparedPhase;const auto& lod=surface.lodSettings();
        if(deformation || k.topologyVersion!=4 || !lod.local_detail_streaming ||
           f.metersPerRadius!=planet.radius*metersPerUnit ||
           glm::length(glm::normalize(k.eye)-f.eyeRadial)*f.metersPerRadius+
               f.radiusMeters+f.transitionMeters>lod.local_detail_radius_m)
            throw std::invalid_argument("Prepared terrain phase requires a matching, covering fine halo and no replay record");
    }
    if(deformation) {
        deformation->validate();
        const auto& g=deformation->generation;
        if(g.field!=k.field || g.fieldVersion!=k.fieldVersion || g.topologyVersion!=k.topologyVersion ||
           deformation->metersPerRadius!=planet.radius*metersPerUnit)
            throw std::invalid_argument("Terrain replay does not match worker field, version or scale");
    }
}
std::shared_ptr<TerrainMorphState> TerrainCpuBuild::previewPhase() const {
    if(!contacts || !deformation || !deformation->phase) return {};
    auto phase=std::make_shared<TerrainMorphState>();phase->set(*deformation->phase);return phase;
}
bool terrainBuildMatches(const TerrainBuildIdentity& a,const TerrainBuildIdentity& b,std::uint64_t installed) {
    return a.epoch==b.epoch && a.bodyIndex==b.bodyIndex && a.bodyName==b.bodyName &&
        a.field==b.field && a.fieldVersion==b.fieldVersion && a.topologyVersion==b.topologyVersion &&
        a.backend==b.backend && a.resident==b.resident && a.localMask==b.localMask &&
        a.serial>installed && a.serial<=b.serial;
}
TerrainCpuBuild buildTerrainCpu(const TerrainBuildRequest& request) {
    request.validate();CpuTrace::Scope scope("terrain.build_land_and_water");
    const auto start=std::chrono::steady_clock::now();
    const auto& surface=request.surface;const auto& planet=request.planet;
    const auto& k=request.identity;const auto& zones=request.previousFaceZones;
    const bool compute=k.backend==TerrainBackend::Compute;
    TerrainCpuBuild result;
    result.deformation=request.deformation;
    const auto sealDeformation=[&](const TerrainTopology& topology) {
        if(request.deformation && !request.deformation->matches(topology.generation,planet.radius*request.metersPerUnit))
            throw std::invalid_argument("Terrain replay topology differs from the rebuilt plan");
        if(request.preparedPhase) {
            if(topology.localDetailLimited || !topology.localDetailTriangles)
                throw std::runtime_error("Prepared terrain fine halo cannot meet its geometry targets");
            result.deformation=TerrainMorphReplay{topology.generation,planet.radius*request.metersPerUnit,*request.preparedPhase};
            result.deformation->validate();
        }
    };
    if(compute) {
        result.field=surface.field();
        auto topology=surface.buildTopologyForEye(k.eye,glm::dvec3(0),zones.empty()?nullptr:&zones,20.0);
        sealDeformation(topology);
        // Contacts validate and own the descriptors. Stage that same immutable
        // topology rather than copying and revalidating it on the GL thread.
        result.contacts=std::make_shared<SparseTerrainContacts>(*result.field,std::move(topology));
        result.topology=SparseTerrainContacts::topologySnapshot(result.contacts);
        if(k.resident || result.deformation) static_cast<TerrainBuildStats&>(result.geometry)=*result.topology;
        else result.geometry=surface.evaluateTopology(*result.topology);
    } else if(request.deformation || request.preparedPhase) {
        auto topology=surface.buildTopologyForEye(k.eye,glm::dvec3(0),zones.empty()?nullptr:&zones,20.0);
        sealDeformation(topology);
        result.geometry=result.deformation->phase?
            surface.evaluateTopology(topology,*result.deformation->phase,TerrainMorphStorage::Retained):
            surface.evaluateTopology(topology,TerrainMorphStorage::Retained);
    } else result.geometry=surface.buildGeometryForEye(k.eye,glm::dvec3(0),zones.empty()?nullptr:&zones,20.0);
    if(planet.water.enabled) {
        auto lod=planet.terrain_lod;
        lod.base_edge_segments=1;lod.medium_edge_segments=3;lod.max_edge_segments=8;lod.steep_edge_segments=8;
        lod.sink_depth_m=0;lod.relief_sinking=false;lod.near_surface_distance_m=lod.shoreline_distance_m;
        lod.geometric_error_m=0;
        lod.local_detail_radius_m=0;lod.local_detail_streaming=false;
        lod.mid_surface_distance_m=2*lod.shoreline_distance_m;lod.max_triangle_budget=std::min(60000,lod.max_triangle_budget);
        const TerrainSurface sea({},lod,planet.radius+planet.water.level_m/request.metersPerUnit,
            request.metersPerUnit,{},0.0);
        if(compute) {
            result.waterField=sea.field();
            result.waterTopology=TerrainTopologySnapshot(sea.buildTopologyForEye(k.eye,glm::dvec3(0)));
            if(k.resident) {result.water.emplace();static_cast<TerrainBuildStats&>(*result.water)=*result.waterTopology;}
            else result.water=sea.evaluateTopology(*result.waterTopology);
        } else result.water=sea.buildGeometryForEye(k.eye,glm::dvec3(0));
    }
    result.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    return result;
}
}
