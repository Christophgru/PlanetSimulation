#include "rendering/geometry/compute/TerrainCompute.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/diagnostics/timing/GpuWorkProfiler.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace rendering {
TerrainComputeLimits TerrainComputeLimits::query() {
    TerrainComputeLimits l;
    if(!GLEW_VERSION_4_3) {l.unavailable="OpenGL 4.3 compute unavailable";return l;}
    GLint bindings=0,blocks=0,size=0,invocations=0,groups=0;GLint64 bytes=0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,&bindings);
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS,&blocks);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,0,&size);
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,&invocations);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,0,&groups);
    glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&bytes);
    if(bindings<8 || blocks<8 || size<64 || invocations<64 || groups<1 || bytes<704)
        l.unavailable="Insufficient compute storage/work-group limits";
    else {l.blockBytes=bytes;l.groups=groups;}
    return l;
}
TerrainComputeBuffers::~TerrainComputeBuffers() {
    if(fence) glDeleteSync(fence);
    glDeleteQueries(2,timers.data());glDeleteBuffers(5,scratch.data());
    if(vao) glDeleteVertexArrays(1,&vao);
    if(vbo) glDeleteBuffers(1,&vbo);
    if(ebo) glDeleteBuffers(1,&ebo);
}
bool TerrainComputeBuffers::poll() {
    if(complete) return true;
    if(!fence) throw std::logic_error("Missing terrain completion fence");
    const auto status=glClientWaitSync(fence,0,0);
    if(status==GL_WAIT_FAILED) throw std::runtime_error("Terrain completion fence failed");
    if(status==GL_TIMEOUT_EXPIRED) return false;
    // A signalled fence usually also makes timestamp results available; check
    // explicitly so the normal polling API never asks for an unavailable result.
    GLint beginReady=0,endReady=0;
    glGetQueryObjectiv(timers[0],GL_QUERY_RESULT_AVAILABLE,&beginReady);
    glGetQueryObjectiv(timers[1],GL_QUERY_RESULT_AVAILABLE,&endReady);
    if(!beginReady || !endReady) return false;
    glDeleteSync(fence);fence=nullptr;complete=true;
    GLuint64 begin=0,end=0;
    glGetQueryObjectui64v(timers[0],GL_QUERY_RESULT,&begin);
    glGetQueryObjectui64v(timers[1],GL_QUERY_RESULT,&end);
    stats.gpuMilliseconds=double(end-begin)/1e6;
    return true;
}
void TerrainComputeBuffers::waitForCapture() {
    // Capture/test callers deliberately wait. Interactive installation is gated
    // until T3 supplies sparse contacts and asynchronous dependent consumers.
    glFlush();
    while(!poll()) {
        const auto status=glClientWaitSync(fence,GL_SYNC_FLUSH_COMMANDS_BIT,1000000);
        if(status==GL_WAIT_FAILED) throw std::runtime_error("Terrain capture wait failed");
    }
}
namespace {
template<class T> std::vector<T> readBuffer(GLuint buffer,std::size_t count) {
    GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glBindBuffer(GL_COPY_READ_BUFFER,buffer);std::vector<T> result(count);
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,count*sizeof(T),result.data());
    glBindBuffer(GL_COPY_READ_BUFFER,previous);return result;
}
}
std::vector<float> TerrainComputeBuffers::readVertices() const {
    if(!complete || !vbo) throw std::logic_error("Terrain vertices are not complete");
    return readBuffer<float>(vbo,stats.gpuCorners*9);
}
std::vector<double> TerrainComputeBuffers::readHeights() const {
    if(!complete || !diagnostics) throw std::logic_error("Terrain diagnostics are unavailable");
    return readBuffer<double>(scratch[3],stats.uniqueSamples);
}
std::vector<std::uint32_t> TerrainComputeBuffers::readIndices() const {
    if(!complete || !ebo) throw std::logic_error("Terrain indices are not complete");
    return readBuffer<std::uint32_t>(ebo,stats.gpuCorners);
}
TerrainCompute::~TerrainCompute() {
    for(auto [key,buffer]:fields_) glDeleteBuffers(1,&buffer);
    for(const auto& shader:variants_) if(shader) glDeleteProgram(shader->id);
    if(morphShader_) glDeleteProgram(morphShader_->id);
}
std::unique_ptr<TerrainComputeBuffers> TerrainCompute::generate(const PlanetField& field,
    const TerrainTopology& topology,bool diagnostics,TerrainMorphStorage morph) {
    topology.validate();
    return generateValidated(field,topology,diagnostics,morph);
}
std::unique_ptr<TerrainComputeBuffers> TerrainCompute::generate(const PlanetField& field,
    const TerrainTopologySnapshot& topology,bool diagnostics,TerrainMorphStorage morph) {
    if(!topology) throw std::invalid_argument("Missing compute terrain snapshot");
    return generateValidated(field,*topology,diagnostics,morph);
}
std::unique_ptr<TerrainComputeBuffers> TerrainCompute::generateValidated(const PlanetField& field,
    const TerrainTopology& topology,bool diagnostics,TerrainMorphStorage morph) {
    if(topology.generation.field!=field.fingerprint()) throw std::invalid_argument("Stale compute planet field");
    if(!limits_.unavailable.empty()) throw std::runtime_error(limits_.unavailable);
    if(morph!=TerrainMorphStorage::Transient && morph!=TerrainMorphStorage::Retained)
        throw std::invalid_argument("Invalid terrain morph storage mode");
    const bool retained=morph==TerrainMorphStorage::Retained;
    if(retained && topology.morph.empty()) throw std::invalid_argument("Retained targets require detail terrain");
    const std::uint64_t samples=topology.samples.size(),corners=topology.indices.size();
    if(!samples || !corners || corners>std::numeric_limits<GLsizei>::max()/9)
        throw std::runtime_error("Terrain generation exceeds addressable output");
    const std::array<std::uint64_t,8> bytes{704,samples*32+topology.morph.size()*128,samples*36+topology.morph.size()*36,corners*4,corners*36,corners*4+(corners/3)*16,
        diagnostics?samples*8:8,sizeof(TerrainSurfacePolicy)*(topology.morph.empty()?1:2)};
    for(auto size:bytes) if(size>limits_.blockBytes || size>std::numeric_limits<GLsizeiptr>::max())
        throw std::runtime_error("Terrain generation exceeds SSBO block limit; use CPU backend");
    if(!limits_.groups) throw std::runtime_error("Zero terrain dispatch capacity");
    const bool detail=!topology.morph.empty();
    auto& variant=variants_[retained?2:detail?1:0];
    if(!variant) variant=std::make_unique<Shader>(retained?"shaders/terrain/compute/detail_targets.comp":
        detail?"shaders/terrain/compute/detail.comp":"shaders/terrain/compute/field.comp");
    if(retained && !morphShader_) morphShader_=std::make_unique<Shader>("shaders/terrain/compute/morph.comp");
    shader_=variant.get();
    GLint previousProgram=0,previousVao=0,previousArray=0,previousStorage=0;
    std::array<GLint,8> previousBindings{};
    std::array<GLint64,8> previousStarts{},previousSizes{};
    glGetIntegerv(GL_CURRENT_PROGRAM,&previousProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previousArray);glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&previousStorage);
    for(int i=0;i<8;++i) {
        glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,i,&previousBindings[i]);
        glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,i,&previousStarts[i]);
        glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,i,&previousSizes[i]);
    }
    struct Restore {
        GLint program,vao,array,storage;std::array<GLint,8> bindings;std::array<GLint64,8> starts,sizes;
        ~Restore() {
            for(int i=0;i<8;++i) {
                if(bindings[i] && sizes[i]>0) glBindBufferRange(GL_SHADER_STORAGE_BUFFER,i,bindings[i],starts[i],sizes[i]);
                else glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,bindings[i]);
            }
            glBindBuffer(GL_SHADER_STORAGE_BUFFER,storage);glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER,array);glUseProgram(program);
        }
    } restore{previousProgram,previousVao,previousArray,previousStorage,previousBindings,previousStarts,previousSizes};
    auto result=std::make_unique<TerrainComputeBuffers>();
    result->stats=static_cast<const TerrainBuildStats&>(topology);
    result->stats.generation.backend=TerrainBackend::Compute;
    result->stats.evaluationQueries={};result->stats.gpuCorners=corners;result->stats.gpuCellKeys=corners/3;result->diagnostics=diagnostics;
    result->retainedMorph=retained;
    result->stats.gpuInputBytes=bytes[1]+corners*4+20+bytes[7]; // source descriptors + rock range/corner count
    result->stats.gpuWorkingBytes=0;for(auto b:bytes) result->stats.gpuWorkingBytes+=b;
    if(!fields_.contains(field.fingerprint())) {
        // Tiny immutable packs, bounded even when repeatedly reloading fields.
        if(fields_.size()==16) {for(auto [key,b]:fields_) glDeleteBuffers(1,&b);fields_.clear();}
        GLuint b=0;glGenBuffers(1,&b);glBindBuffer(GL_SHADER_STORAGE_BUFFER,b);
        glBufferData(GL_SHADER_STORAGE_BUFFER,704,&field.parameters(),GL_STATIC_DRAW);
        if(!b || glGetError()!=GL_NO_ERROR) {
            glDeleteBuffers(1,&b);throw std::runtime_error("Terrain parameter allocation failed");
        }
        try {fields_.emplace(field.fingerprint(),b);} catch(...) {glDeleteBuffers(1,&b);throw;}
        result->stats.gpuInputBytes+=704;
    }
    glGenBuffers(5,result->scratch.data());glGenBuffers(1,&result->vbo);glGenBuffers(1,&result->ebo);
    const std::array<GLuint,8> buffers{fields_.at(field.fingerprint()),result->scratch[0],result->scratch[1],
        result->scratch[2],result->vbo,result->ebo,result->scratch[3],result->scratch[4]};
    for(int i=1;i<8;++i) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffers[i]);
        const void* data=i==1?static_cast<const void*>(topology.samples.data()):i==3?static_cast<const void*>(topology.indices.data()):i==7?static_cast<const void*>(&topology.surfacePolicy):nullptr;
        if((i==1 || i==7) && !topology.morph.empty()) {
            glBufferData(GL_SHADER_STORAGE_BUFFER,bytes[i],nullptr,GL_STATIC_DRAW);
            if(i==1) {
                glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,samples*32,topology.samples.data());
                glBufferSubData(GL_SHADER_STORAGE_BUFFER,samples*32,topology.morph.size()*128,topology.morph.data());
            } else {
                glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,128,&topology.surfacePolicy);
                glBufferSubData(GL_SHADER_STORAGE_BUFFER,128,128,&topology.parentPolicy);
            }
        } else glBufferData(GL_SHADER_STORAGE_BUFFER,bytes[i],data,GL_STATIC_DRAW);
    }
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Terrain GPU buffer allocation failed");
    for(int i=0;i<8;++i) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,buffers[i]);
    shader_->use();
    shader_->setInt("uCorners",corners);
    shader_->setInt("uMorph",!topology.morph.empty());shader_->setInt("uSamples",samples);
    const auto& m=field.parameters().material;
    const double radians=std::acos(-1.0)/180.0;
    glUniform2d(glGetUniformLocation(shader_->id,"uRockRange"),1-std::cos(m[0]*radians),1-std::cos(m[1]*radians));
    glGenQueries(2,result->timers.data());glQueryCounter(result->timers[0],GL_TIMESTAMP);
    const auto dispatch=[&](int phase,std::uint64_t count) {
        const auto chunk=std::min<std::uint64_t>(std::uint64_t(limits_.groups)*64,65536);
        for(std::uint64_t first=0;first<count;first+=chunk) {
            shader_->setInt("uFirst",first);shader_->setInt("uCount",std::min(count,first+chunk));
            shader_->setInt("uPhase",phase);shader_->setInt("uDiagnostics",diagnostics);
            glDispatchCompute((std::min(chunk,count-first)+63)/64,1,1);
            ++result->stats.gpuDispatches;result->stats.gpuInputBytes+=16;
        }
    };
    if(!topology.morph.empty()) {
        GpuWorkProfiler::Scope timing(GpuWorkStage::TerrainField,GpuWorkProfiler::generation(result->stats.generation));
        dispatch(2,3*samples);glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    { GpuWorkProfiler::Scope timing(GpuWorkStage::TerrainField,GpuWorkProfiler::generation(result->stats.generation));
      dispatch(0,samples);glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); }
    GpuWorkProfiler::Scope expansionTiming(GpuWorkStage::TerrainExpansion,GpuWorkProfiler::generation(result->stats.generation));
    dispatch(1,corners);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT|GL_ELEMENT_ARRAY_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT|
        GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
    if(retained) {
        // Repack only after expansion consumed the ordinary unique attributes.
        // Reuse the existing 72 bytes/sample; no new storage or input upload.
        dispatch(3,samples);glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    expansionTiming.stop();
    glQueryCounter(result->timers[1],GL_TIMESTAMP);
    result->fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    if(!result->fence || glGetError()!=GL_NO_ERROR) throw std::runtime_error("Terrain compute submission failed");
    glGenVertexArrays(1,&result->vao);glBindVertexArray(result->vao);
    glBindBuffer(GL_ARRAY_BUFFER,result->vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,result->ebo);
    for(int location=0;location<3;++location) {
        glVertexAttribPointer(location,3,GL_FLOAT,GL_FALSE,36,reinterpret_cast<void*>(std::size_t(location)*12));
        glEnableVertexAttribArray(location);
    }
    if(!result->vao || glGetError()!=GL_NO_ERROR) throw std::runtime_error("Terrain vertex layout allocation failed");
    return result;
}
void TerrainCompute::deform(Mesh& mesh,const TerrainMorphFootprint& footprint) {
    if(!mesh.residentTerrain || !mesh.vbo || !mesh.morphTargets || !mesh.morphCorners ||
       !mesh.indexCount || !mesh.morphSamples || mesh.morphSamples!=mesh.terrainStats.uniqueSamples ||
       mesh.indexCount>std::numeric_limits<GLsizei>::max()/9 ||
       mesh.morphSamples>std::numeric_limits<std::uint32_t>::max()/18)
        throw std::invalid_argument("Terrain mesh has no complete retained morph targets");
    if(mesh.contacts) throw std::logic_error("Bound terrain contacts need a shared deformation phase");
    const auto& eye=footprint.eyeRadial;
    if(!std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z) ||
       std::abs(glm::dot(eye,eye)-1)>1e-12 ||
       !std::isfinite(footprint.metersPerRadius) || footprint.metersPerRadius<=0 ||
       !std::isfinite(footprint.radiusMeters) || footprint.radiusMeters<0 ||
       !std::isfinite(footprint.transitionMeters) || footprint.transitionMeters<=0)
        throw std::invalid_argument("Invalid terrain morph footprint");
    if(!limits_.unavailable.empty() || !limits_.groups) throw std::runtime_error("Terrain deformation compute unavailable");
    if(!morphShader_) morphShader_=std::make_unique<Shader>("shaders/terrain/compute/morph.comp");
    GLint previousProgram=0,previousStorage=0;
    std::array<GLint,3> previousBindings{};
    std::array<GLint64,3> previousStarts{},previousSizes{};
    glGetIntegerv(GL_CURRENT_PROGRAM,&previousProgram);glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&previousStorage);
    for(int i=0;i<3;++i) {
        glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,i,&previousBindings[i]);
        glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,i,&previousStarts[i]);
        glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,i,&previousSizes[i]);
    }
    struct Restore {
        GLint program,storage;std::array<GLint,3> bindings;std::array<GLint64,3> starts,sizes;
        ~Restore() {
            for(int i=0;i<3;++i) {
                if(bindings[i] && sizes[i]>0) glBindBufferRange(GL_SHADER_STORAGE_BUFFER,i,bindings[i],starts[i],sizes[i]);
                else glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,bindings[i]);
            }
            glBindBuffer(GL_SHADER_STORAGE_BUFFER,storage);glUseProgram(program);
        }
    } restore{previousProgram,previousStorage,previousBindings,previousStarts,previousSizes};
    const GLuint buffers[]{mesh.morphTargets,mesh.morphCorners,mesh.vbo};
    for(int i=0;i<3;++i) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,buffers[i]);
    morphShader_->use();morphShader_->setInt("uSamples",mesh.morphSamples);
    glUniform3dv(glGetUniformLocation(morphShader_->id,"uEye"),1,&eye.x);
    glUniform3d(glGetUniformLocation(morphShader_->id,"uFootprint"),
        footprint.metersPerRadius,footprint.radiusMeters,footprint.transitionMeters);
    GpuWorkProfiler::Scope timing(GpuWorkStage::TerrainExpansion,GpuWorkProfiler::generation(mesh.terrainStats.generation));
    const auto chunk=std::min<std::uint64_t>(std::uint64_t(limits_.groups)*64,65536);
    for(std::uint64_t first=0;first<mesh.indexCount;first+=chunk) {
        morphShader_->setInt("uFirst",first);morphShader_->setInt("uCount",std::min(mesh.indexCount,first+chunk));
        glDispatchCompute((std::min<std::uint64_t>(chunk,mesh.indexCount-first)+63)/64,1,1);
    }
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT|GL_SHADER_STORAGE_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Terrain deformation submission failed");
}
}
