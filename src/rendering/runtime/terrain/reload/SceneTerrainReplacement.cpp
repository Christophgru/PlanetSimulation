#include "rendering/runtime/terrain/reload/SceneTerrainReplacement.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include <algorithm>
#include <type_traits>

namespace rendering {
namespace {
app::PreparedScene prepareDocument(const nlohmann::json& document,double time) {
    app::PreparedScene scene(config::ScenarioConfig{config::Config{nlohmann::json(document)}});
    scene.updateSimulation(time);return scene;
}
}
SceneTerrainReplacement::SceneTerrainReplacement(nlohmann::json document,SceneTerrainDestination live,
    std::uint64_t epoch,double time,std::uint64_t limit)
    :SceneTerrainReplacement(document,prepareDocument(document,time),live,epoch,time,limit) {}
SceneTerrainReplacement::SceneTerrainReplacement(nlohmann::json document,app::PreparedScene prepared,
    SceneTerrainDestination live,std::uint64_t epoch,double time,std::uint64_t limit)
    :previousDocument_(live.document),document_(std::move(document)),scene_(std::move(prepared)),
     land_(scene_.scenario.planets.size()),water_(land_.size()),publication_(std::make_unique<TerrainPublication>(grass_,land_.size(),limit)),
     requests_(land_.size()),live_(live),epoch_(epoch),previousEpoch_(live.epoch) {
    if(epoch<=previousEpoch_ || live.publication.pending() || live.publication.externalBytes())
        throw std::invalid_argument("Scene replacement requires a newer epoch and a free preparation/retirement slot");
    if(live.land.size()!=live.scene.scenario.planets.size() || live.water.size()!=live.land.size())
        throw std::invalid_argument("Incomplete live scene consumers");
    for(const auto& planet:scene_.scenario.planets)
        if(planet.foliage.enabled && !planet.foliage.compute_placement)
            throw std::invalid_argument("Resident scene replacement requires GPU foliage placement");
    if(!std::isfinite(time)) throw std::invalid_argument("Invalid scene replacement time");
    publication_->reserveExternal(live.publication.reservedBytes());
    previous_.reserve(live.land.size());
    for(std::size_t i=0;i<live.land.size();++i) previous_.push_back({live.land[i].vbo,live.water[i].vbo,
        live.land[i].vao,live.water[i].vao,live.land[i].ebo,live.water[i].ebo,
        live.land[i].revision,live.water[i].revision,live.land[i].terrainStats.generation,
        live.water[i].terrainStats.generation,live.land[i].contacts.get(),live.scene.scenario.planets[i].name,
        live.scene.terrainSurfaces[i].field().fingerprint(),live.grass.residentGeneration(i),
        live.grass.residentRevision(i),live.grass.planningEye(i)});
    if(!live.publication.acquireSceneReplacement())
        throw std::invalid_argument("Another scene replacement owns the staging/retirement slot");
}
SceneTerrainReplacement::~SceneTerrainReplacement() {
    if(retirementFence_) glDeleteSync(retirementFence_);
    // Publication and grass die before Mesh objects. Delete grass's buffer views
    // before the terrain handles they reference; programs outlive these views.
    publication_.reset();grass_.clear();
    for(auto& m:land_) m.destroy();
    for(auto& m:water_) m.destroy();
    if(!published_) live_.publication.releaseSceneReplacement();
}
TerrainBuildRequest SceneTerrainReplacement::request(std::size_t i,const glm::dvec3& eye,std::uint64_t serial,
    std::vector<int> zones) {
    return requestLocal(i,scene_.bodies.at(i+1).toLocalPoint(eye),serial,std::move(zones));
}
TerrainBuildRequest SceneTerrainReplacement::requestLocal(std::size_t i,const glm::dvec3& eye,std::uint64_t serial,
    std::vector<int> zones) {
    if(published_ || preparing_ || requests_.at(i)) throw std::logic_error("Scene body request already owned or preparation busy");
    const auto& planet=scene_.scenario.planets[i];
    TerrainBuildIdentity k;k.epoch=epoch_;k.serial=serial;k.bodyIndex=i;k.bodyName=planet.name;
    k.field=scene_.terrainSurfaces[i].field().fingerprint();k.backend=TerrainBackend::Compute;k.resident=true;
    k.eye=eye;k.localMask=glm::length(k.eye)<3*planet.radius ? 1 : 0;
    TerrainBuildRequest r{k,scene_.terrainSurfaces[i],planet,std::move(zones),scene_.scenario.metersPerWorldUnit()};
    r.validate();requests_[i]=k;return r;
}
bool SceneTerrainReplacement::submit(TerrainCpuBuild build,const TerrainBuildIdentity& k,
    const glm::dvec3& eye,TerrainCompute& compute) {
    if(published_ || preparing_) return false;
    if(k.bodyIndex>=requests_.size() || requests_[k.bodyIndex]!=k ||
       publication_->installed(k.bodyIndex).identity.serial)
        throw std::invalid_argument("Obsolete or changed replacement body identity");
    const auto i=k.bodyIndex;
    if(!publication_->submit(std::move(build),k,scene_.scenario.planets[i],scene_.scenario.metersPerWorldUnit(),
        eye,land_[i],water_[i],compute)) return false;
    preparing_=i;return true;
}
bool SceneTerrainReplacement::poll() {
    if(published_) throw std::logic_error("Scene already published");
    publication_->pollRetired();
    if(preparing_ && publication_->poll()) {
        const auto i=*preparing_;
        if(!publication_->publish(*requests_[i],land_[i],water_[i]))
            throw std::logic_error("Replacement body became obsolete");
        preparing_.reset();
    }
    return ready();
}
void SceneTerrainReplacement::waitForCapture() {
    if(preparing_) publication_->waitForCapture();
    (void)poll();
}
bool SceneTerrainReplacement::ready() const noexcept {
    if(published_ || preparing_) return false;
    for(std::size_t i=0;i<requests_.size();++i)
        if(!requests_[i] || publication_->installed(i).identity!=*requests_[i]) return false;
    return true;
}
void SceneTerrainReplacement::validateLive() const {
    if(live_.epoch!=previousEpoch_ || live_.document!=previousDocument_ ||
       live_.land.size()!=previous_.size() || live_.water.size()!=previous_.size() ||
       live_.scene.scenario.planets.size()!=previous_.size() || live_.scene.terrainSurfaces.size()!=previous_.size() || live_.publication.pending() ||
       live_.publication.reservedBytes()>publication_->externalBytes())
        throw std::invalid_argument("Live scene changed during replacement preparation");
    for(std::size_t i=0;i<previous_.size();++i) {
        const auto& p=previous_[i];
        if(live_.land[i].vbo!=p.land || live_.water[i].vbo!=p.water ||
           live_.land[i].vao!=p.landVao || live_.water[i].vao!=p.waterVao ||
           live_.land[i].ebo!=p.landEbo || live_.water[i].ebo!=p.waterEbo ||
           live_.land[i].revision!=p.landRevision || live_.water[i].revision!=p.waterRevision ||
           live_.land[i].contacts.get()!=p.contacts || live_.scene.scenario.planets[i].name!=p.name ||
           live_.scene.terrainSurfaces[i].field().fingerprint()!=p.field ||
           live_.land[i].terrainStats.generation!=p.landKey || live_.water[i].terrainStats.generation!=p.waterKey ||
           live_.grass.residentGeneration(i)!=p.grassKey || live_.grass.residentRevision(i)!=p.grassRevision ||
           live_.grass.planningEye(i)!=p.grassEye)
            throw std::invalid_argument("Live scene consumers changed during replacement preparation");
    }
}
bool SceneTerrainReplacement::publish(std::uint64_t currentEpoch) {
    if(currentEpoch!=epoch_) return false;
    if(!ready()) throw std::logic_error("Complete replacement scene is not ready");
    validateLive();
    for(std::size_t i=0;i<land_.size();++i) {
        const auto& r=publication_->installed(i);
        if(land_[i].terrainStats.generation!=r.land || water_[i].terrainStats.generation!=r.water ||
           !land_[i].contacts || land_[i].contacts->generation()!=r.land ||
           grass_.residentGeneration(i)!=r.land || grass_.residentRevision(i)!=r.landRevision ||
           grass_.planningEye(i)!=r.grassEye)
            throw std::invalid_argument("Incomplete replacement scene consumers");
    }
    retirementFence_=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    if(!retirementFence_) throw std::runtime_error("Scene retirement fence allocation failed");
    // Everything after the last fallible operation is a no-throw exchange.
    static_assert(std::is_nothrow_swappable_v<app::PreparedScene>);
    static_assert(std::is_nothrow_swappable_v<nlohmann::json>);
    using std::swap;swap(scene_,live_.scene);document_.swap(live_.document);
    land_.swap(live_.land);water_.swap(live_.water);grass_.swapState(live_.grass);
    publication_->swapInstalledState(live_.publication);live_.epoch=epoch_;published_=true;
    return true;
}
bool SceneTerrainReplacement::pollRetired() {
    if(!published_) throw std::logic_error("Scene has not been exchanged");
    if(retired_) return true;
    const auto status=glClientWaitSync(retirementFence_,0,0);
    if(status==GL_WAIT_FAILED) throw std::runtime_error("Scene retirement fence poll failed");
    if(status==GL_TIMEOUT_EXPIRED) return false;
    live_.publication.pollRetired();
    publication_.reset();grass_.clear();
    for(auto& m:land_) m.destroy();
    for(auto& m:water_) m.destroy();
    glDeleteSync(retirementFence_);retirementFence_=nullptr;retired_=true;
    live_.publication.reserveExternal(0);live_.publication.releaseSceneReplacement();return true;
}
void SceneTerrainReplacement::waitRetiredForCapture() {
    if(!published_) throw std::logic_error("Scene has not been exchanged");
    while(!pollRetired()) {
        const auto status=glClientWaitSync(retirementFence_,GL_SYNC_FLUSH_COMMANDS_BIT,1000000);
        if(status==GL_WAIT_FAILED) throw std::runtime_error("Scene retirement fence wait failed");
    }
}
}
