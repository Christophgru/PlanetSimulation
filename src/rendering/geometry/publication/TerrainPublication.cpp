#include "rendering/geometry/publication/TerrainPublication.h"
#include "rendering/diagnostics/timing/GpuWorkProfiler.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "rendering/foliage/planning/GrassMetadata.h"
#include <algorithm>
#include <limits>
#include <type_traits>

namespace rendering {
namespace {
struct MeshIdentity {
    const Mesh* address;
    GLuint vao,vbo,ebo;
    std::uint64_t revision;
    TerrainGenerationKey generation;
    const SparseTerrainContacts* contacts;
    explicit MeshIdentity(const Mesh& m):address(&m),vao(m.vao),vbo(m.vbo),ebo(m.ebo),
        revision(m.revision),generation(m.terrainStats.generation),contacts(m.contacts.get()) {}
    bool matches(const Mesh& m) const noexcept {
        return address==&m && vao==m.vao && vbo==m.vbo && ebo==m.ebo && revision==m.revision &&
            generation==m.terrainStats.generation && contacts==m.contacts.get();
    }
};
}
struct TerrainPublication::Stage {
    TerrainPublishedGeneration receipt;
    MeshIdentity previousLand,previousWater;
    std::optional<TerrainGenerationKey> previousGrass;
    std::optional<std::uint64_t> previousGrassRevision;
    std::optional<glm::dvec3> previousGrassEye;
    Mesh land,water;
    std::unique_ptr<TerrainGpuPreparation> terrain;
    std::unique_ptr<ProceduralGrass::Preparation> grass;
    GLsync retirementFence=nullptr;
    bool ready=false,grassOnly=false;
    Stage(const Mesh& l,const Mesh& w):previousLand(l),previousWater(w) {}
    ~Stage() {
        if(retirementFence) glDeleteSync(retirementFence);
        // Grass buffer textures refer to the terrain; destroy them first.
        grass.reset();land.destroy();water.destroy();
    }
};
TerrainPublication::TerrainPublication(ProceduralGrass& grass,std::size_t bodies,
    std::uint64_t totalLimit,std::uint64_t perSetLimit)
    :grass_(grass),bodies_(bodies),totalLimit_(totalLimit),perSetLimit_(perSetLimit) {
    if(!totalLimit || !perSetLimit) throw std::invalid_argument("Empty terrain publication budget");
    for(std::size_t i=0;i<bodies;++i) grass_.reserve(i);
}
TerrainPublication::~TerrainPublication()=default;
bool TerrainPublication::canSubmit(std::size_t i) const noexcept {
    return i<bodies_.size() && !staged_ && !bodies_[i].retiring && !sceneReplacementOwned_;
}
std::uint64_t TerrainPublication::reservedBytes() const noexcept {
    std::uint64_t bytes=externalBytes_+(staged_ ? staged_->receipt.admittedBytes : 0);
    for(const auto& body:bodies_) {
        bytes+=body.installed.admittedBytes;
        if(body.retiring) bytes+=body.retiring->receipt.admittedBytes;
    }
    return bytes;
}
void TerrainPublication::reserveExternal(std::uint64_t bytes) {
    const auto own=reservedBytes()-externalBytes_;
    if(bytes>totalLimit_ || own>totalLimit_-bytes)
        throw std::runtime_error("Scene replacement overlap exceeds publication budget");
    externalBytes_=bytes;
    stats_.peakReservedBytes=std::max(stats_.peakReservedBytes,reservedBytes());
}
bool TerrainPublication::acquireSceneReplacement() noexcept {
    if(sceneReplacementOwned_ || staged_ || externalBytes_) return false;
    sceneReplacementOwned_=true;return true;
}
void TerrainPublication::swapInstalledState(TerrainPublication& other) noexcept {
    cancel();other.cancel();bodies_.swap(other.bodies_);
    using std::swap;swap(totalLimit_,other.totalLimit_);swap(perSetLimit_,other.perSetLimit_);
    swap(externalBytes_,other.externalBytes_);swap(stats_,other.stats_);
}
bool TerrainPublication::submit(TerrainCpuBuild build,const TerrainBuildIdentity& k,
    const config::PlanetConfig& planet,double metersPerUnit,const glm::dvec3& grassEye,
    const Mesh& liveLand,const Mesh& liveWater,TerrainCompute& compute) {
    if(k.bodyIndex>=bodies_.size()) throw std::out_of_range("Terrain publication body");
    if(!canSubmit(k.bodyIndex)) return false;
    GpuWorkProfiler::Request timingIdentity(k.epoch,k.serial,k.bodyIndex);
    try {
        const auto& installed=bodies_[k.bodyIndex].installed;
        if(!k.resident || liveLand.revision!=installed.landRevision || liveWater.revision!=installed.waterRevision ||
           (!installed.identity.serial && (liveLand.vao || liveWater.vao || liveLand.contacts || liveWater.contacts ||
            grass_.patches_[k.bodyIndex]->ready)) ||
           (installed.identity.serial && (liveLand.terrainStats.generation!=installed.land ||
            liveWater.terrainStats.generation!=installed.water || !liveLand.contacts ||
            liveLand.contacts->generation()!=installed.land || grass_.residentGeneration(k.bodyIndex)!=installed.land ||
            grass_.residentRevision(k.bodyIndex)!=installed.landRevision || grass_.planningEye(k.bodyIndex)!=installed.grassEye)) ||
           liveLand.revision==std::numeric_limits<std::uint64_t>::max() ||
           liveWater.revision==std::numeric_limits<std::uint64_t>::max())
            throw std::invalid_argument("Untracked or incompatible publication destination");
        // Reject overlap before dispatching or allocating GPU resources.
        // Grass uses the live context's block limit, even if the terrain
        // evaluator was constructed with a deliberately smaller test limit.
        const auto limits=TerrainComputeLimits::query();
        const auto reserved=reservedBytes();
        const auto requested=TerrainGpuPreparation::requiredBytes(build,k,planet,limits);
        const auto triangles=build.topology->triangleCount();
        const auto fixed=requested-ProceduralGrass::stageBytes(triangles,planet.foliage,limits.blockBytes);
        const auto effective=grass_.budgetPlanet(planet,triangles,limits.blockBytes,
            reserved,std::min(perSetLimit_,totalLimit_-std::min(totalLimit_,reserved)),fixed);
        const auto bytes=fixed+ProceduralGrass::stageBytes(triangles,effective.foliage,limits.blockBytes);
        if(bytes>perSetLimit_ || reserved>totalLimit_ || bytes>totalLimit_-reserved)
            throw std::runtime_error("Complete terrain generations exceed publication budget");
        auto next=std::make_unique<Stage>(liveLand,liveWater);
        next->previousGrass=grass_.residentGeneration(k.bodyIndex);
        next->previousGrassRevision=grass_.residentRevision(k.bodyIndex);
        next->previousGrassEye=grass_.planningEye(k.bodyIndex);
        next->receipt.identity=k;next->receipt.admittedBytes=bytes;
        next->receipt.landRevision=liveLand.revision+1;next->receipt.waterRevision=liveWater.revision+1;
        next->receipt.grassEye=grassEye;next->receipt.foliage=effective.foliage;next->receipt.faceZones=build.geometry.faceZones;
        next->receipt.zoneFaces=build.geometry.zoneFaces;next->receipt.steepRefinedFaces=build.geometry.steepRefinedFaces;
        next->receipt.waterEnabled=bool(build.water);
        next->receipt.preparedRadiusMeters=build.preparedRadiusMeters;
        next->terrain=std::make_unique<TerrainGpuPreparation>(std::move(build),k,effective,compute,perSetLimit_);
        auto& t=*next->terrain;
        next->grass=grass_.submitResident(t.land->vbo,t.land->ebo,t.land->stats,effective,metersPerUnit,grassEye,
            next->receipt.landRevision,t.land->stats.gpuWorkingBytes+(t.water ? t.water->stats.gpuWorkingBytes : 0),perSetLimit_,
            t.land->morphSource());
        if(next->grass->admittedBytes>bytes) throw std::logic_error("Grass reservation exceeds complete generation admission");
        staged_=std::move(next);++stats_.submitted;
        stats_.peakReservedBytes=std::max(stats_.peakReservedBytes,reservedBytes());
        return true;
    } catch(...) {++stats_.failed;throw;}
}
bool TerrainPublication::submitCpu(TerrainCpuBuild build,const TerrainBuildIdentity& k,
    const config::PlanetConfig& planet,double metersPerUnit,const glm::dvec3& grassEye,
    const Mesh& liveLand,const Mesh& liveWater) {
    if(k.bodyIndex>=bodies_.size()) throw std::out_of_range("Terrain publication body");
    if(!canSubmit(k.bodyIndex)) return false;
    try {
        const auto& installed=bodies_[k.bodyIndex].installed;const auto& patch=*grass_.patches_[k.bodyIndex];
        const auto& g=build.geometry.generation;
        if(k.resident || k.backend!=TerrainBackend::Cpu || !k.epoch || !k.serial || k.serial<=installed.identity.serial ||
           k.bodyName!=planet.name || k.field!=g.field || !g.field || !g.topology ||
           k.fieldVersion!=g.fieldVersion || g.fieldVersion!=PlanetField::version ||
           k.topologyVersion!=g.topologyVersion || !supportedTerrainTopology(g.topologyVersion) || g.backend!=TerrainBackend::Cpu ||
           !std::isfinite(glm::length(k.eye)) || glm::length(k.eye)<=0 || (k.localMask!=0 && k.localMask!=1) ||
           !std::isfinite(metersPerUnit) || metersPerUnit<=0 || !std::isfinite(planet.radius*metersPerUnit) || planet.radius<=0 ||
           bool(build.water)!=planet.water.enabled || (build.water &&
               (build.water->generation.backend!=TerrainBackend::Cpu || !build.water->generation.field || !build.water->generation.topology)) ||
           liveLand.revision!=installed.landRevision || liveWater.revision!=installed.waterRevision ||
           (!installed.identity.serial && (liveLand.vao || liveWater.vao || patch.patches || patch.ready)) ||
           (installed.identity.serial && (installed.identity.backend!=TerrainBackend::Cpu ||
               liveLand.terrainStats.generation!=installed.land || liveWater.terrainStats.generation!=installed.water ||
               patch.generation!=installed.land || patch.revision!=installed.landRevision || patch.eye!=installed.grassEye ||
               patch.morphSource!=liveLand.morphSource() || patch.morphState!=liveLand.morphState)) ||
           liveLand.revision==std::numeric_limits<std::uint64_t>::max() || liveWater.revision==std::numeric_limits<std::uint64_t>::max())
            throw std::invalid_argument("Missing matching CPU publication consumers");
        const auto validateGeometry=[](const TerrainGeometry& geometry) {
            if(geometry.indices.empty() || geometry.indices.size()%3 || geometry.vertices.size()!=9*geometry.indices.size() ||
               (!geometry.grassCellKeys.empty() && geometry.grassCellKeys.size()*3!=geometry.indices.size()) ||
               geometry.generation.fieldVersion!=PlanetField::version || !supportedTerrainTopology(geometry.generation.topologyVersion))
                throw std::invalid_argument("Invalid expanded CPU terrain publication geometry");
            for(std::size_t i=0;i<geometry.indices.size();++i) if(geometry.indices[i]!=i)
                throw std::invalid_argument("CPU publication requires expanded sequential corners");
            for(float value:geometry.vertices) if(!std::isfinite(value))
                throw std::invalid_argument("CPU publication requires finite terrain vertices");
        };
        validateGeometry(build.geometry);if(build.water) validateGeometry(*build.water);
        grassMetadataParameters(planet,metersPerUnit,grassEye,build.geometry.indices.size()/3);
        if(g.field!=PlanetField(planet.surface_noise,planet.radius,metersPerUnit,planet.terrain_landscape,
               planet.water.enabled?std::optional(planet.water.level_m):std::nullopt,planet.terrain_material).fingerprint() ||
           (build.water && build.water->generation.field!=PlanetField({},planet.radius+planet.water.level_m/metersPerUnit,metersPerUnit,{},0.).fingerprint()))
            throw std::invalid_argument("CPU publication field differs from configured land or water");
        if(build.deformation) {
            build.deformation->validate();
            if(!build.deformation->matches(g,planet.radius*metersPerUnit) || !build.geometry.morphData ||
               build.geometry.morphData->metersPerRadius!=planet.radius*metersPerUnit ||
               build.geometry.morphData->phase!=build.deformation->phase)
                throw std::invalid_argument("CPU publication phase differs from retained source");
        } else if(build.geometry.morphData) throw std::invalid_argument("CPU retained publication needs a sealed phase record");
        if(build.preparedRadiusMeters!=0 && (build.preparedRadiusMeters!=planet.terrain_lod.local_detail_radius_m ||
           !build.deformation || !build.deformation->phase || build.geometry.localDetailLimited || !build.geometry.localDetailTriangles ||
           !terrainPhaseFitsPreparation(*build.deformation->phase,k.eye,planet.radius*metersPerUnit,build.preparedRadiusMeters)))
            throw std::invalid_argument("CPU publication phase has no matching fine preparation coverage");
        if(planet.foliage.enabled && !grass_.adaptiveBudget_)
            throw std::invalid_argument("CPU publication requires protected foliage admission");
        const auto bytes=[](const TerrainGeometry& geometry) {
            return geometry.vertices.size()*sizeof(float)+geometry.indices.size()*sizeof(unsigned)+
                geometry.grassCellKeys.size()*16+(geometry.morphData?geometry.morphData->bytes():0);
        };
        const auto terrainBytes=bytes(build.geometry)+(build.water?bytes(*build.water):0),reserved=reservedBytes();
        const auto allowance=std::min(perSetLimit_,totalLimit_-std::min(totalLimit_,reserved));
        if(terrainBytes>allowance) throw std::runtime_error("CPU terrain exceeds complete publication budget");
        const auto triangles=build.geometry.indices.size()/3;
        auto effective=planet;
        const bool cached=GLEW_VERSION_4_3 && effective.foliage.compute_placement;
        if(effective.foliage.enabled) {
            GrassFalloff minimum;minimum.enabled=true;
            const auto fixed=grass_.cpuStageBytes(triangles,cached,minimum),candidateBytes=cached?grassComputedCandidateBytes:128;
            if(allowance-terrainBytes<fixed+candidateBytes) throw std::runtime_error("CPU foliage exceeds complete publication budget");
            effective.foliage.max_blades=std::min<std::uint64_t>(effective.foliage.max_blades,(allowance-terrainBytes-fixed)/candidateBytes);
        }
        const auto external=grass_.externalBudgetBytes_,grassReserved=grass_.reservedBytes();
        const auto supplement=reserved>grassReserved?reserved-grassReserved:0;
        if(terrainBytes>std::numeric_limits<std::uint64_t>::max()-external ||
           supplement>std::numeric_limits<std::uint64_t>::max()-external-terrainBytes)
            throw std::overflow_error("CPU publication overlap overflow");
        struct Restore {ProceduralGrass& grass;std::uint64_t external;~Restore(){grass.externalBudgetBytes(external);}} restore{grass_,external};
        grass_.externalBudgetBytes_=external+supplement+terrainBytes;
        const auto policy=grass_.cpuPolicy(effective,triangles);
        const auto admitted=terrainBytes+grass_.cpuStageBytes(triangles,cached,policy);
        if(admitted>allowance) throw std::runtime_error("Complete CPU generation exceeds publication budget");
        auto next=std::make_unique<Stage>(liveLand,liveWater);
        next->previousGrass=grass_.residentGeneration(k.bodyIndex);next->previousGrassRevision=grass_.residentRevision(k.bodyIndex);
        next->previousGrassEye=grass_.planningEye(k.bodyIndex);
        auto& receipt=next->receipt;receipt.identity=k;receipt.admittedBytes=admitted;
        receipt.landRevision=liveLand.revision+1;receipt.waterRevision=liveWater.revision+1;
        receipt.foliage=effective.foliage;receipt.faceZones=std::move(build.geometry.faceZones);receipt.zoneFaces=build.geometry.zoneFaces;
        receipt.steepRefinedFaces=build.geometry.steepRefinedFaces;receipt.waterEnabled=bool(build.water);
        receipt.preparedRadiusMeters=build.preparedRadiusMeters;
        next->land.loadTerrain(std::move(build.geometry));
        if(build.preparedRadiusMeters>0) next->land.prepareCpuDeformation();
        if(build.water) next->water.loadTerrain(std::move(*build.water));
        next->land.revision=receipt.landRevision;next->water.revision=receipt.waterRevision;
        if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("CPU terrain publication upload failed");
        grass_.externalBudgetBytes_=external+supplement;
        next->grass=grass_.submitCpu(k.bodyIndex,next->land,effective,metersPerUnit,grassEye,terrainBytes);
        if(next->grass->admittedBytes!=admitted) throw std::logic_error("CPU grass reservation differs from complete admission");
        grass_.validateCommit(k.bodyIndex,*next->grass,next->land);
        receipt.grassEye=next->grass->patch_->eye;receipt.land=next->land.terrainStats.generation;receipt.water=next->water.terrainStats.generation;
        receipt.triangles=next->land.indexCount/3;next->ready=true;
        staged_=std::move(next);++stats_.submitted;stats_.peakReservedBytes=std::max(stats_.peakReservedBytes,reservedBytes());
        return true;
    } catch(...) {++stats_.failed;throw;}
}
bool TerrainPublication::poll() {
    if(!staged_) return false;
    if(staged_->ready) return true;
    try {
        auto& s=*staged_;
        if(s.grassOnly) {
            if(!grass_.poll(*s.grass)) return false;
            grass_.validateCommit(s.receipt.identity.bodyIndex,*s.grass,*s.previousLand.address);
            s.ready=true;return true;
        }
        auto& t=*s.terrain;
        const bool terrainReady=t.poll(),grassReady=grass_.poll(*s.grass);
        if(!terrainReady || !grassReady) return false;
        s.land.loadComputedTerrain(std::move(t.cpu.geometry),*t.land,false);
        if(t.water) s.water.loadComputedTerrain(std::move(*t.cpu.water),*t.water,false);
        s.land.revision=s.receipt.landRevision;s.water.revision=s.receipt.waterRevision;
        if(!s.land.contacts || s.land.contacts->generation()!=s.land.terrainStats.generation)
            throw std::invalid_argument("Publication requires matching terrain contacts");
        grass_.validateCommit(s.receipt.identity.bodyIndex,*s.grass,s.land);
        s.receipt.land=s.land.terrainStats.generation;s.receipt.water=s.water.terrainStats.generation;
        s.receipt.triangles=s.land.indexCount/3;
        s.terrain.reset();s.ready=true;return true;
    } catch(...) {staged_.reset();++stats_.failed;throw;}
}
void TerrainPublication::waitForCapture() {
    if(!staged_) throw std::logic_error("Missing capture publication");
    if(staged_->ready) return;
    try {
        if(staged_->terrain) staged_->terrain->waitForCapture();
        grass_.waitForCapture(*staged_->grass);
    } catch(...) {staged_.reset();++stats_.failed;throw;}
    if(!poll()) throw std::logic_error("Incomplete capture publication");
}
bool TerrainPublication::updatePreparedPhase(std::size_t body,const TerrainMorphFootprint& phase,TerrainCompute& compute) {
    if(body>=bodies_.size()) throw std::out_of_range("Terrain publication body");
    if(!staged_ || !staged_->ready || staged_->grassOnly || staged_->receipt.identity.bodyIndex!=body ||
       staged_->receipt.preparedRadiusMeters==0) return false;
    auto& s=*staged_;
    if(!s.land.contacts || !terrainPhaseFitsPreparation(phase,s.receipt.identity.eye,
       s.land.contacts->radiusMeters(),s.receipt.preparedRadiusMeters))
        throw std::invalid_argument("Updated terrain phase is outside its prepared fine halo");
    if(s.land.morphState && s.land.morphState->footprint() && *s.land.morphState->footprint()==phase) return true;
    const auto& identity=s.receipt.identity;
    GpuWorkProfiler::Request timingIdentity(identity.epoch,identity.serial,body);
    try {compute.deform(s.land,phase);}
    catch(...) {staged_.reset();++stats_.failed;throw;}
    // Immutable roots remain valid. commitPrepared attaches this shared state;
    // its first draw refreshes current planes/animation using the new revision.
    return true;
}
bool TerrainPublication::updatePreparedPhase(std::size_t body,const TerrainMorphFootprint& phase) {
    if(body>=bodies_.size()) throw std::out_of_range("Terrain publication body");
    if(!staged_ || !staged_->ready || staged_->grassOnly || staged_->receipt.identity.bodyIndex!=body ||
       staged_->receipt.identity.backend!=TerrainBackend::Cpu || staged_->receipt.preparedRadiusMeters==0) return false;
    auto& stage=*staged_;
    if(!stage.land.morphData || !terrainPhaseFitsPreparation(phase,stage.receipt.identity.eye,
       stage.land.morphData->metersPerRadius,stage.receipt.preparedRadiusMeters))
        throw std::invalid_argument("Updated CPU terrain phase is outside its prepared fine halo");
    const auto& identity=stage.receipt.identity;
    GpuWorkProfiler::Request timingIdentity(identity.epoch,identity.serial,body);
    try {stage.land.deformCpu(phase);}
    catch(...) {staged_.reset();++stats_.failed;throw;}
    return true;
}
bool TerrainPublication::updateInstalledPhase(std::size_t body,const TerrainMorphFootprint& phase,Mesh& land) {
    if(body>=bodies_.size()) throw std::out_of_range("Terrain publication body");
    const auto& installed=bodies_[body].installed;
    if(sceneReplacementOwned_ || !installed.identity.serial || installed.identity.backend!=TerrainBackend::Cpu ||
       installed.preparedRadiusMeters==0) return false;
    const auto& patch=*grass_.patches_[body];
    if(land.residentTerrain || land.contacts || !land.morphData || !land.morphState || !land.morphState->footprint() ||
       land.revision!=installed.landRevision || land.terrainStats.generation!=installed.land ||
       land.morphData->generation!=installed.land || patch.generation!=installed.land || patch.revision!=installed.landRevision ||
       patch.morphSource!=land.morphSource() || patch.morphState!=land.morphState)
        throw std::invalid_argument("Installed CPU deformation consumers do not match their prepared generation");
    if(!terrainPhaseFitsPreparation(phase,installed.identity.eye,land.morphData->metersPerRadius,installed.preparedRadiusMeters))
        throw std::invalid_argument("Updated CPU terrain phase is outside its installed fine halo");
    const auto& identity=installed.identity;
    GpuWorkProfiler::Request timingIdentity(identity.epoch,identity.serial,body);
    try {land.deformCpu(phase);}
    catch(...) {++stats_.failed;throw;} // The CPU adapter restores old data or requests recovery.
    return true;
}
bool TerrainPublication::updateInstalledPhase(std::size_t body,const TerrainMorphFootprint& phase,Mesh& land,TerrainCompute& compute) {
    if(body>=bodies_.size()) throw std::out_of_range("Terrain publication body");
    const auto& installed=bodies_[body].installed;
    if(sceneReplacementOwned_ || !installed.identity.serial || installed.preparedRadiusMeters==0) return false;
    const auto& patch=*grass_.patches_[body];
    if(!land.residentTerrain || !land.contacts || !land.morphState || !land.morphState->footprint() ||
       land.revision!=installed.landRevision || land.terrainStats.generation!=installed.land ||
       land.contacts->generation()!=installed.land || land.morphSamples!=land.terrainStats.uniqueSamples ||
       patch.morphSource!=land.morphSource() || patch.morphState!=land.morphState ||
       grass_.residentGeneration(body)!=installed.land || grass_.residentRevision(body)!=installed.landRevision)
        throw std::invalid_argument("Installed deformation consumers do not match their prepared generation");
    if(!terrainPhaseFitsPreparation(phase,installed.identity.eye,land.contacts->radiusMeters(),installed.preparedRadiusMeters))
        throw std::invalid_argument("Updated terrain phase is outside its installed fine halo");
    const auto previous=*land.morphState->footprint();
    if(previous==phase) return true;
    const auto& identity=installed.identity;
    GpuWorkProfiler::Request timingIdentity(identity.epoch,identity.serial,body);
    try {compute.deform(land,phase);}
    catch(...) {
        ++stats_.failed;
        // deform publishes state only after successful submission. Immutable
        // targets can restore the old VBO without a mirror, allocation or wait.
        try {compute.deform(land,previous);}
        catch(...) {throw std::runtime_error("Installed terrain phase rollback failed; renderer recovery required");}
        throw;
    }
    return true;
}
bool TerrainPublication::publish(const TerrainBuildIdentity& current,Mesh& liveLand,Mesh& liveWater) {
    if(!staged_ || !staged_->ready) throw std::logic_error("Publication is not ready");
    auto& s=*staged_;const auto i=s.receipt.identity.bodyIndex;auto& body=bodies_[i];
    const auto installedSerial=body.installed.identity.serial-(s.grassOnly ? 1 : 0);
    if(!terrainBuildMatches(s.receipt.identity,current,installedSerial)) {cancel();return false;}
    if(body.retiring || !s.previousLand.matches(liveLand) || !s.previousWater.matches(liveWater) ||
       grass_.residentGeneration(i)!=s.previousGrass || grass_.residentRevision(i)!=s.previousGrassRevision ||
       grass_.planningEye(i)!=s.previousGrassEye)
        throw std::invalid_argument("Live terrain changed during preparation");
    grass_.validateCommit(i,*s.grass,s.grassOnly ? liveLand : s.land);
    // This fence follows the old generation's final draws in the same context.
    // Allocate it before touching any live consumer. Failure is fully reversible.
    s.retirementFence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    if(!s.retirementFence) {
        staged_.reset();++stats_.failed;
        throw std::runtime_error("Terrain retirement fence allocation failed");
    }
    static_assert(std::is_nothrow_swappable_v<TerrainPublishedGeneration>);
    if(!s.grassOnly) {liveLand.swap(s.land);liveWater.swap(s.water);}
    grass_.commitPrepared(i,*s.grass,liveLand);
    using std::swap;swap(body.installed,s.receipt);
    if(s.grassOnly) ++stats_.grassOnlyPublished;
    body.retiring=std::move(staged_);++stats_.published;
    return true;
}
void TerrainPublication::cancel() noexcept {
    if(staged_) {staged_.reset();++stats_.obsolete;}
}
void TerrainPublication::pollRetired() {
    for(auto& body:bodies_) if(body.retiring) {
        const auto status=glClientWaitSync(body.retiring->retirementFence,0,0);
        if(status==GL_WAIT_FAILED) throw std::runtime_error("Terrain retirement fence poll failed");
        if(status!=GL_TIMEOUT_EXPIRED) {body.retiring.reset();++stats_.retired;}
    }
}
bool TerrainPublication::ready() const noexcept {return staged_ && staged_->ready;}
bool TerrainPublication::retiring(std::size_t i) const {return bool(bodies_.at(i).retiring);}
const TerrainPublishedGeneration& TerrainPublication::installed(std::size_t i) const {return bodies_.at(i).installed;}
bool TerrainPublication::reuseGrassPolicy(std::size_t i,const config::PlanetConfig& planet,double units,
    const Mesh& land,const Mesh& water) {
    if(!canSubmit(i) || !land.residentTerrain || land.terrainStats.generation!=bodies_.at(i).installed.land ||
        water.terrainStats.generation!=bodies_.at(i).installed.water) return false;
    const auto reserved=reservedBytes(),occupied=land.terrainStats.gpuWorkingBytes+water.terrainStats.gpuWorkingBytes;
    try {
        const auto effective=grass_.budgetPlanet(planet,land.indexCount/3,TerrainComputeLimits::query().blockBytes,reserved,
            std::min(perSetLimit_,totalLimit_-std::min(totalLimit_,reserved)),occupied);
        return grass_.reuseResidentPolicy(i,land,effective,units);
    } catch(const std::runtime_error&) {
        return false; // Normal submission reports admission failure and keeps the live plan.
    }
}
bool TerrainPublication::submitGrass(std::size_t i,const config::PlanetConfig& planet,double units,
    const glm::dvec3& eye,const Mesh& land,const Mesh& water) {
    if(i>=bodies_.size()) throw std::out_of_range("Grass publication body");
    if(!canSubmit(i)) return false;
    try {
        const auto& old=bodies_[i].installed;
        GpuWorkProfiler::Request timingIdentity(old.identity.epoch,old.identity.serial,i);
        if(!old.identity.serial || old.identity.bodyName!=planet.name || !land.residentTerrain ||
           land.revision!=old.landRevision || water.revision!=old.waterRevision ||
           land.terrainStats.generation!=old.land || water.terrainStats.generation!=old.water ||
           !land.contacts || land.contacts->generation()!=old.land ||
           grass_.residentGeneration(i)!=old.land || grass_.residentRevision(i)!=old.landRevision ||
           grass_.planningEye(i)!=old.grassEye || old.waterEnabled!=planet.water.enabled)
            throw std::invalid_argument("Untracked grass publication source");
        const auto occupied=land.terrainStats.gpuWorkingBytes+water.terrainStats.gpuWorkingBytes;
        const auto reserved=reservedBytes();
        const auto block=TerrainComputeLimits::query().blockBytes;
        const auto effective=grass_.budgetPlanet(planet,land.indexCount/3,block,reserved,
            std::min(perSetLimit_,totalLimit_-std::min(totalLimit_,reserved)),occupied);
        const auto bytes=occupied+ProceduralGrass::stageBytes(land.indexCount/3,effective.foliage,block);
        if(bytes>perSetLimit_ || reserved>totalLimit_ || bytes>totalLimit_-reserved)
            throw std::runtime_error("Grass replacement exceeds publication budget");
        auto next=std::make_unique<Stage>(land,water);next->grassOnly=true;next->receipt=old;
        next->receipt.grassEye=eye;next->receipt.foliage=effective.foliage;next->receipt.admittedBytes=bytes;
        next->previousGrass=grass_.residentGeneration(i);next->previousGrassRevision=grass_.residentRevision(i);
        next->previousGrassEye=grass_.planningEye(i);
        next->grass=grass_.submitResident(land.vbo,land.ebo,land.terrainStats,effective,units,eye,land.revision,occupied,perSetLimit_,land.morphSource());
        staged_=std::move(next);++stats_.submitted;
        stats_.peakReservedBytes=std::max(stats_.peakReservedBytes,reservedBytes());return true;
    } catch(...) {++stats_.failed;throw;}
}
void TerrainPublication::waitRetiredForCapture(std::size_t i) {
    auto& body=bodies_.at(i);
    while(body.retiring) {
        const auto status=glClientWaitSync(body.retiring->retirementFence,GL_SYNC_FLUSH_COMMANDS_BIT,1000000);
        if(status==GL_WAIT_FAILED) throw std::runtime_error("Capture retirement wait failed");
        pollRetired();
    }
}
}
