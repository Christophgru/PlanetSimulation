#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "rendering/foliage/planning/GrassMetadata.h"
#include "rendering/foliage/planning/GrassAllocation.h"
#include "rendering/geometry/Mesh.h"
#include <algorithm>
#include <limits>
#include <utility>
#include <cstdio>
#include "config/ScenarioConfig.h"

namespace rendering {
bool ProceduralGrass::policyChanged(std::size_t i) const {
    return adaptiveBudget_ && i<patches_.size() && patches_[i] && patches_[i]->falloff.enabled &&
        !patches_[i]->falloff.locked && patches_[i]->policyRevision!=budget_.revision();
}
nlohmann::json ProceduralGrass::policy(std::size_t i) const {
    return i<patches_.size() && patches_[i] && patches_[i]->falloff.enabled ? patches_[i]->falloff.json() : nlohmann::json(nullptr);
}
void ProceduralGrass::restorePolicies(const nlohmann::json& replay,const std::vector<config::PlanetConfig>& planets) {
    replayPolicies_.clear();
    if(!adaptiveBudget_) return; // Historical captures retain their recorded legacy profile.
    if(!replay.contains("render") || !replay.at("render").contains("foliage_policy")) return;
    const auto& policies=replay.at("render").at("foliage_policy");
    if(!policies.is_array() || policies.size()!=planets.size())
        throw std::invalid_argument("Foliage policy replay requires matching bodies");
    for(std::size_t i=0;i<planets.size();++i) {
        if(policies[i].is_null()) {
            if(planets[i].foliage.enabled) throw std::invalid_argument("Missing enabled-body foliage policy replay");
        } else {
            if(!planets[i].foliage.enabled || policies[i].at("body")!=planets[i].name)
                throw std::invalid_argument("Foliage policy replay body mismatch");
            replayPolicies_.emplace(planets[i].name,GrassFalloff::replay(policies[i],planets[i].foliage));
        }
    }
}
config::PlanetConfig ProceduralGrass::budgetPlanet(const config::PlanetConfig& planet,std::uint64_t n,std::uint64_t block,
    std::uint64_t reserved,std::uint64_t logical,std::uint64_t terrainBytes) const {
    auto result=planet;
    if(!adaptiveBudget_ || !planet.foliage.enabled) return result;
    auto minimal=planet.foliage;minimal.max_blades=1;
    const auto fixed=terrainBytes+stageBytes(n,minimal,block)-128;
    const auto available=budget_.available(reserved,logical);
    if(available<fixed+128) throw std::runtime_error("Insufficient foliage staging budget (terrain/metadata + one slot)");
    const auto capacity=std::min<std::uint64_t>({std::uint64_t(planet.foliage.max_blades),block/128,(available-fixed)/128});
    if(const auto saved=replayPolicies_.find(planet.name);saved!=replayPolicies_.end()) {
        if(saved->second.capacity>capacity) throw std::runtime_error("Locked foliage replay exceeds available staging memory");
        result.foliage.max_blades=saved->second.capacity;
    } else result.foliage.max_blades=capacity;
    return result;
}
ProceduralGrass::Preparation::Preparation():patch_(std::make_unique<Patch>()) {}
ProceduralGrass::Preparation::~Preparation() {if(resourcesFence_) glDeleteSync(resourcesFence_);}
ProceduralGrassStats ProceduralGrass::Preparation::stats() const {return patchStats(*patch_);}
std::uint64_t ProceduralGrass::stageBytes(std::uint64_t n,const config::FoliageConfig& f,std::uint64_t blockBytes) {
    f.validate();if(!f.enabled) return 0;
    if(!n || n>std::numeric_limits<std::uint32_t>::max()/9 || !blockBytes)
        throw std::invalid_argument("Invalid grass staging capacity");
    const auto groups=(n+63)/64,budget=std::min<std::uint64_t>(f.max_blades,blockBytes/128);
    return n*64+160 + 224+groups*96+n*4+n*8+groups*68 + budget*128+32;
}
std::unique_ptr<ProceduralGrass::Preparation> ProceduralGrass::submitResident(GLuint vertices,GLuint indices,
    const TerrainBuildStats& terrain,const config::PlanetConfig& planet,double metersPerWorldUnit,
    const glm::dvec3& eyeBody,std::uint64_t revision,std::uint64_t otherBytes,std::uint64_t byteLimit) {
    if(terrain.generation.backend!=TerrainBackend::Compute || !terrain.generation.field || !terrain.generation.topology ||
       terrain.generation.fieldVersion!=PlanetField::version || terrain.generation.topologyVersion!=1 ||
       !terrain.gpuCorners || terrain.gpuCorners%3 || !vertices || !indices || !glIsBuffer(vertices) || !glIsBuffer(indices))
        throw std::invalid_argument("Invalid resident grass source");
    // Validate all settings/eye inputs even for the explicit disabled consumer.
    grassMetadataParameters(planet,metersPerWorldUnit,eyeBody,terrain.gpuCorners/3);
    const auto limits=TerrainComputeLimits::query();
    if(!limits.unavailable.empty()) throw std::runtime_error(limits.unavailable);
    const auto bytes=stageBytes(terrain.gpuCorners/3,planet.foliage,limits.blockBytes);
    if(otherBytes>byteLimit || bytes>byteLimit-otherBytes)
        throw std::runtime_error("Resident grass exceeds logical staging byte limit");
    if(planet.foliage.enabled && !planet.foliage.compute_placement)
        throw std::invalid_argument("Resident grass requires compute placement");
    auto result=std::make_unique<Preparation>();auto& patch=*result->patch_;
    result->generation_=terrain.generation;result->vertices_=vertices;result->indices_=indices;
    result->admittedBytes=otherBytes+bytes;
    patch.stageAdmittedBytes=result->admittedBytes;
    patch.settings=planet.foliage;patch.color={planet.color[0],planet.color[1],planet.color[2]};
    const auto rock=planet.terrain_material.slopeMetricRange();patch.rockRange={rock[0],rock[1]};
    double relief=planet.terrain_landscape.maximumAbsoluteHeightMeters();
    for(const auto& noise:planet.surface_noise) relief+=noise.amplitude_m;
    patch.landscapeLevels={planet.water.enabled?planet.water.level_m:0,.1,relief};
    patch.water=planet.water.enabled;patch.landscape=planet.terrain_landscape.enabled;
    patch.scale=planet.radius*metersPerWorldUnit;patch.seed=planet.foliage.seed;
    patch.eye=eyeBody;patch.revision=revision;
    patch.policyRevision=budget_.revision();
    if(adaptiveBudget_ && planet.foliage.enabled) {
        if(const auto saved=replayPolicies_.find(planet.name);saved!=replayPolicies_.end()) patch.falloff=saved->second;
        else {
            auto& p=patch.falloff;p.enabled=true;
            p.capacity=std::min<std::uint64_t>(planet.foliage.max_blades,limits.blockBytes/128);
            p.budget=budget_.softBudget(p.capacity,planet.foliage);
            p.protectedMeters=planet.foliage.quadDistanceMeters();
            p.sigmaMeters=planet.foliage.draw_distance_m*planet.foliage.gaussian_sigma_fraction;
            p.density=planet.foliage.density_per_m2;
        }
    }
    patch.generation=terrain.generation;
    patch.workIdentity=GpuWorkProfiler::generation(terrain.generation);
    if(!planet.foliage.enabled) {patch.ready=true;result->ready_=true;return result;}
    if(!metadataCompute_) metadataCompute_=std::make_unique<GrassMetadataCompute>();
    if(!allocationCompute_) allocationCompute_=std::make_unique<GrassAllocationCompute>();
    patch.metadata=metadataCompute_->generate(vertices,indices,terrain,planet,metersPerWorldUnit,eyeBody,patch.falloff);
    patch.allocation=allocationCompute_->generate(*patch.metadata,planet.foliage);
    // GPU command order/barriers carry the terrain -> metadata -> allocation
    // dependency. No CPU wait or readback is needed here.
    return result;
}
bool ProceduralGrass::poll(Preparation& p) {
    if(p.failed_) throw std::logic_error("Failed or consumed grass preparation");
    if(p.ready_) return true;
    try {
        auto& patch=*p.patch_;
        if(!p.summaryConsumed_) {
            if(!patch.allocation->poll()) return false;
            if(!patch.metadata->poll()) return false;
            if(patch.metadata->generation!=p.generation_ || patch.allocation->generation!=p.generation_ ||
               patch.metadata->planningEye!=patch.eye || patch.allocation->planningEye!=patch.eye)
                throw std::logic_error("Stale staged grass consumers");
            const auto summary=patch.allocation->readSummary();
            if(summary.densitySearch[0]>patch.settings.density_per_m2)
                throw std::runtime_error("Staged grass density exceeds requested density");
            patch.draws.reserve(grassCandidateSlots.size());
            for(std::size_t level=0;level<grassCandidateSlots.size();++level) {
                patch.batches[level].count=summary.counts[level]*grassCandidateSlots[level];
                if(summary.counts[level]) patch.draws.push_back({summary.first[level],summary.counts[level],int(level),6});
            }
            patch.density=summary.densitySearch[0];patch.allocationBudget=summary.control[0];
            if(patch.falloff.enabled) {
                patch.falloff.density=patch.density;patch.falloff.budget=patch.allocationBudget;
                patch.falloff.sigmaMeters=summary.densitySearch[3];patch.falloff.nearInfeasible=summary.padding[0]!=0;
            }
            patch.patches=summary.totals[0];patch.distanceMeters=patch.settings.draw_distance_m;
            patch.buffer=std::exchange(patch.allocation->references,0);p.summaryConsumed_=true;
            // All fallible resource creation happens before ready/commit.
            GLint vao=0,array=0,active=0,storage=0;std::array<GLint,2> textures{};
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
            glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&storage);
            glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
            for(int i=0;i<2;++i) {glActiveTexture(GL_TEXTURE8+i);glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&textures[i]);}
            struct Restore {
                GLint vao,array,active,storage;std::array<GLint,2> textures;
                ~Restore() {
                    for(int i=0;i<2;++i) {glActiveTexture(GL_TEXTURE8+i);glBindTexture(GL_TEXTURE_BUFFER,textures[i]);}
                    glActiveTexture(active);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,array);
                    glBindBuffer(GL_SHADER_STORAGE_BUFFER,storage);
                }
            } restore{vao,array,active,storage,textures};
            glGenTextures(1,&patch.vertexTexture);glGenTextures(1,&patch.indexTexture);
            glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_BUFFER,patch.vertexTexture);glTexBuffer(GL_TEXTURE_BUFFER,GL_R32F,p.vertices_);
            glActiveTexture(GL_TEXTURE9);glBindTexture(GL_TEXTURE_BUFFER,patch.indexTexture);glTexBuffer(GL_TEXTURE_BUFFER,GL_R32UI,p.indices_);
            if(summary.totals[1] && !compute_) compute_=std::make_unique<Shader>("shaders/foliage/placement.comp");
            allocateComputed(patch,summary.totals[1]);
            if(!patch.vertexTexture || !patch.indexTexture || glGetError()!=GL_NO_ERROR)
                throw std::runtime_error("Grass draw resource allocation failed");
            p.resourcesFence_=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
            if(!p.resourcesFence_) throw std::runtime_error("Grass resource completion fence failed");
        }
        const auto status=glClientWaitSync(p.resourcesFence_,0,0);
        if(status==GL_WAIT_FAILED) throw std::runtime_error("Grass resource completion failed");
        if(status==GL_TIMEOUT_EXPIRED) return false;
        glDeleteSync(p.resourcesFence_);p.resourcesFence_=nullptr;
        patch.ready=true;p.ready_=true;return true;
    } catch(...) {p.failed_=true;throw;}
}
void ProceduralGrass::waitForCapture(Preparation& p) {
    glFlush();
    if(p.patch_->allocation && !p.summaryConsumed_) p.patch_->allocation->waitForCapture();
    while(!poll(p)) {
        glFlush();
        if(p.resourcesFence_ && glClientWaitSync(p.resourcesFence_,GL_SYNC_FLUSH_COMMANDS_BIT,1000000)==GL_WAIT_FAILED)
            throw std::runtime_error("Grass resource capture wait failed");
    }
}
void ProceduralGrass::validateCommit(std::size_t index,const Preparation& p,const Mesh& mesh) const {
    if(!p.ready_ || p.failed_ || index>=patches_.size() || !patches_[index] ||
       mesh.terrainStats.generation!=p.generation_ || mesh.vbo!=p.vertices_ || mesh.ebo!=p.indices_ || mesh.revision!=p.patch_->revision)
        throw std::invalid_argument("Grass commit requires reserved matching ready consumers");
}
void ProceduralGrass::commitPrepared(std::size_t index,Preparation& p) noexcept {
    const auto& next=p.patch_->falloff;
    const auto& old=patches_[index]->falloff;
    if(next.nearInfeasible && (!old.nearInfeasible || next.capacity!=old.capacity))
        std::fprintf(stderr,"Foliage near density infeasible: capacity=%u density=%.6g protected_m=%.6g\n",
            next.capacity,next.density,next.protectedMeters);
    patches_[index].swap(p.patch_);p.ready_=false;p.failed_=true;
    // The previous patch remains owned by p. Whole-generation publication keeps
    // it with the old terrain until the last-use retirement fence signals.
}
void ProceduralGrass::commit(std::size_t index,Preparation& p,const Mesh& mesh) {
    validateCommit(index,p,mesh);
    commitPrepared(index,p);
}
std::optional<TerrainGenerationKey> ProceduralGrass::residentGeneration(std::size_t index) const {
    if(index>=patches_.size() || !patches_[index] || !patches_[index]->ready ||
       patches_[index]->generation.backend!=TerrainBackend::Compute) return std::nullopt;
    return patches_[index]->generation;
}
std::optional<std::uint64_t> ProceduralGrass::residentRevision(std::size_t index) const {
    if(!residentGeneration(index)) return std::nullopt;
    return patches_[index]->revision;
}
}
