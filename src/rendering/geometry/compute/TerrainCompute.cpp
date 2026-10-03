#include "rendering/geometry/compute/TerrainCompute.h"
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
    if(bindings<7 || blocks<7 || size<64 || invocations<64 || groups<1 || bytes<704)
        l.unavailable="Insufficient compute storage/work-group limits";
    else {l.blockBytes=bytes;l.groups=groups;}
    return l;
}
TerrainComputeBuffers::~TerrainComputeBuffers() {
    if(fence) glDeleteSync(fence);
    glDeleteQueries(2,timers.data());glDeleteBuffers(4,scratch.data());
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
    if(shader_) glDeleteProgram(shader_->id);
}
std::unique_ptr<TerrainComputeBuffers> TerrainCompute::generate(const PlanetField& field,
    const TerrainTopology& topology,bool diagnostics) {
    topology.validate();
    if(topology.generation.field!=field.fingerprint()) throw std::invalid_argument("Stale compute planet field");
    if(!limits_.unavailable.empty()) throw std::runtime_error(limits_.unavailable);
    const std::uint64_t samples=topology.samples.size(),corners=topology.indices.size();
    if(!samples || !corners || corners>std::numeric_limits<GLsizei>::max()/9)
        throw std::runtime_error("Terrain generation exceeds addressable output");
    const std::array<std::uint64_t,7> bytes{704,samples*32,samples*36,corners*4,corners*36,corners*4,
        diagnostics?samples*8:8};
    for(auto size:bytes) if(size>limits_.blockBytes || size>std::numeric_limits<GLsizeiptr>::max())
        throw std::runtime_error("Terrain generation exceeds SSBO block limit; use CPU backend");
    if(!limits_.groups) throw std::runtime_error("Zero terrain dispatch capacity");
    if(!shader_) shader_=std::make_unique<Shader>("shaders/terrain/compute/field.comp");
    GLint previousProgram=0,previousVao=0,previousArray=0,previousStorage=0;
    std::array<GLint,7> previousBindings{};
    glGetIntegerv(GL_CURRENT_PROGRAM,&previousProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previousArray);glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&previousStorage);
    for(int i=0;i<7;++i) glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,i,&previousBindings[i]);
    struct Restore {
        GLint program,vao,array,storage;std::array<GLint,7> bindings;
        ~Restore() {
            for(int i=0;i<7;++i) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,bindings[i]);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER,storage);glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER,array);glUseProgram(program);
        }
    } restore{previousProgram,previousVao,previousArray,previousStorage,previousBindings};
    auto result=std::make_unique<TerrainComputeBuffers>();
    result->stats=static_cast<const TerrainBuildStats&>(topology);
    result->stats.generation.backend=TerrainBackend::Compute;
    result->stats.evaluationQueries={};result->stats.gpuCorners=corners;result->diagnostics=diagnostics;
    result->stats.gpuInputBytes=samples*32+corners*4+16; // source descriptors + double rock range
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
    glGenBuffers(4,result->scratch.data());glGenBuffers(1,&result->vbo);glGenBuffers(1,&result->ebo);
    const std::array<GLuint,7> buffers{fields_.at(field.fingerprint()),result->scratch[0],result->scratch[1],
        result->scratch[2],result->vbo,result->ebo,result->scratch[3]};
    for(int i=1;i<7;++i) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffers[i]);
        const void* data=i==1?static_cast<const void*>(topology.samples.data()):i==3?static_cast<const void*>(topology.indices.data()):nullptr;
        glBufferData(GL_SHADER_STORAGE_BUFFER,bytes[i],data,GL_STATIC_DRAW);
    }
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Terrain GPU buffer allocation failed");
    for(int i=0;i<7;++i) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,buffers[i]);
    shader_->use();
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
    dispatch(0,samples);glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    dispatch(1,corners);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT|GL_ELEMENT_ARRAY_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT|
        GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
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
}
