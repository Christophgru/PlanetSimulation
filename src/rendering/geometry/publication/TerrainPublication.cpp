#include "rendering/geometry/publication/TerrainPublication.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
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
        const auto bytes=TerrainGpuPreparation::requiredBytes(build,k,planet,TerrainComputeLimits::query());
        const auto reserved=reservedBytes();
        if(bytes>perSetLimit_ || reserved>totalLimit_ || bytes>totalLimit_-reserved)
            throw std::runtime_error("Complete terrain generations exceed publication budget");
        auto next=std::make_unique<Stage>(liveLand,liveWater);
        next->previousGrass=grass_.residentGeneration(k.bodyIndex);
        next->previousGrassRevision=grass_.residentRevision(k.bodyIndex);
        next->previousGrassEye=grass_.planningEye(k.bodyIndex);
        next->receipt.identity=k;next->receipt.admittedBytes=bytes;
        next->receipt.landRevision=liveLand.revision+1;next->receipt.waterRevision=liveWater.revision+1;
        next->receipt.grassEye=grassEye;next->receipt.foliage=planet.foliage;next->receipt.faceZones=build.geometry.faceZones;
        next->receipt.zoneFaces=build.geometry.zoneFaces;next->receipt.steepRefinedFaces=build.geometry.steepRefinedFaces;
        next->receipt.waterEnabled=bool(build.water);
        next->terrain=std::make_unique<TerrainGpuPreparation>(std::move(build),k,planet,compute,perSetLimit_);
        auto& t=*next->terrain;
        next->grass=grass_.submitResident(t.land->vbo,t.land->ebo,t.land->stats,planet,metersPerUnit,grassEye,
            next->receipt.landRevision,t.land->stats.gpuWorkingBytes+(t.water ? t.water->stats.gpuWorkingBytes : 0),perSetLimit_);
        if(next->grass->admittedBytes>bytes) throw std::logic_error("Grass reservation exceeds complete generation admission");
        staged_=std::move(next);++stats_.submitted;
        stats_.peakReservedBytes=std::max(stats_.peakReservedBytes,reservedBytes());
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
    grass_.commitPrepared(i,*s.grass);
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
bool TerrainPublication::submitGrass(std::size_t i,const config::PlanetConfig& planet,double units,
    const glm::dvec3& eye,const Mesh& land,const Mesh& water) {
    if(i>=bodies_.size()) throw std::out_of_range("Grass publication body");
    if(!canSubmit(i)) return false;
    try {
        const auto& old=bodies_[i].installed;
        if(!old.identity.serial || old.identity.bodyName!=planet.name || !land.residentTerrain ||
           land.revision!=old.landRevision || water.revision!=old.waterRevision ||
           land.terrainStats.generation!=old.land || water.terrainStats.generation!=old.water ||
           !land.contacts || land.contacts->generation()!=old.land ||
           grass_.residentGeneration(i)!=old.land || grass_.residentRevision(i)!=old.landRevision ||
           grass_.planningEye(i)!=old.grassEye || old.waterEnabled!=planet.water.enabled)
            throw std::invalid_argument("Untracked grass publication source");
        const auto occupied=land.terrainStats.gpuWorkingBytes+water.terrainStats.gpuWorkingBytes;
        const auto bytes=occupied+ProceduralGrass::stageBytes(land.indexCount/3,planet.foliage,TerrainComputeLimits::query().blockBytes);
        const auto reserved=reservedBytes();
        if(bytes>perSetLimit_ || reserved>totalLimit_ || bytes>totalLimit_-reserved)
            throw std::runtime_error("Grass replacement exceeds publication budget");
        auto next=std::make_unique<Stage>(land,water);next->grassOnly=true;next->receipt=old;
        next->receipt.grassEye=eye;next->receipt.foliage=planet.foliage;next->receipt.admittedBytes=bytes;
        next->previousGrass=grass_.residentGeneration(i);next->previousGrassRevision=grass_.residentRevision(i);
        next->previousGrassEye=grass_.planningEye(i);
        next->grass=grass_.submitResident(land.vbo,land.ebo,land.terrainStats,planet,units,eye,land.revision,occupied,perSetLimit_);
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
