#include "rendering/foliage/planning/GrassMetadata.h"
#include "rendering/diagnostics/timing/GpuWorkProfiler.h"
#include "rendering/foliage/GrassPlacement.h"
#include "config/ScenarioConfig.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace rendering {
GrassMetadataParameters grassMetadataParameters(const config::PlanetConfig& planet,
    double metersPerWorldUnit,const glm::dvec3& eye,std::uint32_t triangles,const GrassFalloff& policy) {
    planet.foliage.validate();
    const double scale=planet.radius*metersPerWorldUnit;
    if(!std::isfinite(scale) || scale<=0 || !std::isfinite(glm::length(eye)) || glm::length(eye)<=0)
        throw std::invalid_argument("Invalid grass metadata scale or eye");
    const auto& f=planet.foliage;
    const double margin=grassRebuildDistance(f),variance2=2*std::pow(f.draw_distance_m*f.gaussian_sigma_fraction,2);
    const double weightedArea=std::acos(-1.0)*variance2*(1-std::exp(-std::pow(f.draw_distance_m+margin,2)/variance2));
    const double density=std::min(f.density_per_m2,f.budget_fraction*f.max_blades/weightedArea);
    double maximumHeight=planet.terrain_landscape.maximumAbsoluteHeightMeters();
    for(const auto& noise:planet.surface_noise) maximumHeight+=noise.amplitude_m;
    const double water=planet.water.enabled?planet.water.level_m:0,relief=std::max(1.0,maximumHeight-water);
    const double snowStart=std::max(water+1.1,water+.25*relief),snowEnd=std::max(snowStart+1,water+.4*relief);
    GrassMetadataParameters p;
    p.eyeScale={eye.x,eye.y,eye.z,scale};p.ranges={f.draw_distance_m,margin,variance2,density};
    p.biome={water,f.water_clearance_m,planet.terrain_material.slopeMetricRange()[1],snowEnd};
    p.colorGreen={planet.color[0],planet.color[1],planet.color[2],f.green_ratio};
    p.flags={std::uint32_t(f.enabled && (glm::length(eye)-1)*scale-maximumHeight<=f.draw_distance_m+margin),
        std::uint32_t(planet.water.enabled),std::uint32_t(planet.terrain_landscape.enabled),triangles};
    if(policy.enabled) {
        p.ranges[3]=policy.locked?policy.density:f.density_per_m2;
        p.ranges[2]=policy.sigmaMeters;
        p.padding={policy.locked?(3u|(policy.nearInfeasible?4u:0u)):1u,std::bit_cast<std::uint32_t>(float(policy.protectedMeters)),
            0u,policy.budget};
    }
    return p;
}
GrassMetadataLimits GrassMetadataLimits::query() {
    GrassMetadataLimits l;
    if(!GLEW_VERSION_4_3) {l.unavailable="OpenGL 4.3 grass metadata unavailable";return l;}
    GLint bindings=0,blocks=0,size=0,invocations=0,groups=0;GLint64 bytes=0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,&bindings);
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS,&blocks);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,0,&size);
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,&invocations);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,0,&groups);
    glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&bytes);
    if(bindings<4 || blocks<4 || size<64 || invocations<64 || groups<1 || bytes<160)
        l.unavailable="Insufficient grass metadata storage/work-group limits";
    else {l.blockBytes=bytes;l.groups=groups;}
    return l;
}
GrassMetadataBuffers::~GrassMetadataBuffers() {
    if(fence) glDeleteSync(fence);
    glDeleteBuffers(1,&descriptors);glDeleteBuffers(1,&parameters);
}
bool GrassMetadataBuffers::poll() {
    if(complete) return true;
    if(!fence) throw std::logic_error("Missing grass metadata fence");
    const auto status=glClientWaitSync(fence,0,0);
    if(status==GL_WAIT_FAILED) throw std::runtime_error("Grass metadata completion failed");
    if(status==GL_TIMEOUT_EXPIRED) return false;
    glDeleteSync(fence);fence=nullptr;complete=true;return true;
}
void GrassMetadataBuffers::waitForCapture() {
    glFlush();
    while(!poll()) {
        if(glClientWaitSync(fence,GL_SYNC_FLUSH_COMMANDS_BIT,1000000)==GL_WAIT_FAILED)
            throw std::runtime_error("Grass metadata capture wait failed");
    }
}
std::vector<GrassTriangleMetadata> GrassMetadataBuffers::readForValidation() const {
    if(!complete || !descriptors) throw std::logic_error("Grass metadata is not complete");
    GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
    glBindBuffer(GL_COPY_READ_BUFFER,descriptors);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    std::vector<GrassTriangleMetadata> result(triangles);
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,triangles*sizeof(GrassTriangleMetadata),result.data());
    glBindBuffer(GL_COPY_READ_BUFFER,previous);
    diagnosticReadBytes+=triangles*sizeof(GrassTriangleMetadata);return result;
}
GrassMetadataCompute::~GrassMetadataCompute() {if(shader_) glDeleteProgram(shader_->id);}
std::unique_ptr<GrassMetadataBuffers> GrassMetadataCompute::generate(GLuint vertices,GLuint indices,
    const TerrainBuildStats& terrain,const config::PlanetConfig& planet,double metersPerWorldUnit,const glm::dvec3& eye,const GrassFalloff& policy) {
    if(terrain.generation.backend!=TerrainBackend::Compute || !terrain.generation.field || !terrain.generation.topology ||
        terrain.generation.fieldVersion!=PlanetField::version || !supportedTerrainTopology(terrain.generation.topologyVersion) ||
        !vertices || !indices || !glIsBuffer(vertices) || !glIsBuffer(indices) ||
        !terrain.gpuCorners || terrain.gpuCorners%3 || terrain.gpuCorners>std::numeric_limits<std::uint32_t>::max()/9)
        throw std::invalid_argument("Invalid grass metadata terrain generation");
    if(!limits_.unavailable.empty()) throw std::runtime_error(limits_.unavailable);
    if(!limits_.groups) throw std::runtime_error("Zero grass metadata dispatch capacity");
    const auto count=terrain.gpuCorners/3;
    const std::array<std::uint64_t,4> bytes{160,terrain.gpuCorners*36,terrain.gpuCorners*4,count*64};
    for(auto size:bytes) if(size>limits_.blockBytes || size>std::uint64_t(std::numeric_limits<GLsizeiptr>::max()))
        throw std::runtime_error("Grass metadata exceeds SSBO block limit");
    const auto parameters=grassMetadataParameters(planet,metersPerWorldUnit,eye,count,policy);
    if(!shader_) shader_=std::make_unique<Shader>("shaders/foliage/planning/metadata.comp");
    // Preserve indexed ranges as well as buffer IDs; caller state can use ranges.
    GLint program=0,storage=0;std::array<GLint,4> bindings{};std::array<GLint64,4> starts{},sizes{};
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&storage);
    for(int i=0;i<4;++i) {
        glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,i,&bindings[i]);
        glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,i,&starts[i]);
        glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,i,&sizes[i]);
    }
    struct Restore {
        GLint program,storage;std::array<GLint,4> bindings;std::array<GLint64,4> starts,sizes;
        ~Restore() {
            for(int i=0;i<4;++i) {
                if(bindings[i] && sizes[i]>0) glBindBufferRange(GL_SHADER_STORAGE_BUFFER,i,bindings[i],starts[i],sizes[i]);
                else glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,bindings[i]);
            }
            glBindBuffer(GL_SHADER_STORAGE_BUFFER,storage);glUseProgram(program);
        }
    } restore{program,storage,bindings,starts,sizes};
    // Check the actual resident source sizes, before allocating or dispatching.
    for(const auto [buffer,needed]:{std::pair{vertices,bytes[1]},std::pair{indices,bytes[2]}}) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer);GLint64 actual=0;
        glGetBufferParameteri64v(GL_SHADER_STORAGE_BUFFER,GL_BUFFER_SIZE,&actual);
        if(actual<0 || std::uint64_t(actual)<needed) throw std::invalid_argument("Truncated grass metadata terrain buffer");
    }
    auto result=std::make_unique<GrassMetadataBuffers>();
    result->adaptive=policy.enabled;result->generation=terrain.generation;result->planningEye=eye;result->triangles=count;
    result->inputBytes=160;result->workingBytes=160+count*64;
    glGenBuffers(1,&result->parameters);glGenBuffers(1,&result->descriptors);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,result->parameters);glBufferData(GL_SHADER_STORAGE_BUFFER,160,&parameters,GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,result->descriptors);glBufferData(GL_SHADER_STORAGE_BUFFER,count*64,nullptr,GL_STATIC_DRAW);
    if(!result->parameters || !result->descriptors || glGetError()!=GL_NO_ERROR)
        throw std::runtime_error("Grass metadata allocation failed");
    const std::array<GLuint,4> buffers{result->parameters,vertices,indices,result->descriptors};
    for(int i=0;i<4;++i) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,buffers[i]);
    shader_->use();glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    GpuWorkProfiler::Scope timing(GpuWorkStage::GrassMetadata,GpuWorkProfiler::generation(terrain.generation));
    const auto chunk=std::min<std::uint64_t>(std::uint64_t(limits_.groups)*64,65536);
    for(std::uint64_t first=0;first<count;first+=chunk) {
        glUniform1ui(glGetUniformLocation(shader_->id,"uFirst"),first);
        glUniform1ui(glGetUniformLocation(shader_->id,"uEnd"),std::min(count,first+chunk));
        glDispatchCompute((std::min(chunk,count-first)+63)/64,1,1);
        ++result->dispatches;result->inputBytes+=8;
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
    timing.stop();
    result->fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    if(!result->fence || glGetError()!=GL_NO_ERROR) throw std::runtime_error("Grass metadata submission failed");
    return result;
}
}
