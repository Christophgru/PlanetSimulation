#include "rendering/foliage/planning/GrassAllocation.h"
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
namespace rendering {
GrassAllocationBuffers::~GrassAllocationBuffers() {
    if(fence) glDeleteSync(fence);
    for(auto b:{references,state,groups,ranks,offsets}) glDeleteBuffers(1,&b);
}
bool GrassAllocationBuffers::poll() {
    if(complete) return true;
    if(!fence) throw std::logic_error("Missing grass allocation fence");
    const auto status=glClientWaitSync(fence,0,0);
    if(status==GL_WAIT_FAILED) throw std::runtime_error("Grass allocation completion failed");
    if(status==GL_TIMEOUT_EXPIRED) return false;
    glDeleteSync(fence);fence=nullptr;complete=true;return true;
}
void GrassAllocationBuffers::waitForCapture() {
    glFlush();while(!poll()) if(glClientWaitSync(fence,GL_SYNC_FLUSH_COMMANDS_BIT,1000000)==GL_WAIT_FAILED)
        throw std::runtime_error("Grass allocation capture wait failed");
}
namespace {
void read(GLuint buffer,std::size_t bytes,void* output) {
    GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
    glBindBuffer(GL_COPY_READ_BUFFER,buffer);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    if(bytes) glGetBufferSubData(GL_COPY_READ_BUFFER,0,bytes,output);
    glBindBuffer(GL_COPY_READ_BUFFER,previous);
}
}
GrassAllocationSummary GrassAllocationBuffers::readSummary() const {
    if(!complete) throw std::logic_error("Grass allocation is not complete");
    GrassAllocationSummary s;read(state,sizeof(s),&s);summaryReadBytes+=sizeof(s);
    if(s.totals[2] || s.totals[1]>s.control[0] || s.totals[0]>triangles ||
        !std::isfinite(s.densitySearch[0]) || s.densitySearch[0]<0)
        throw std::runtime_error("Invalid GPU grass budget summary");
    return s;
}
std::vector<std::uint32_t> GrassAllocationBuffers::readReferencesForValidation(std::size_t count) const {
    if(!complete || count>triangles) throw std::logic_error("Invalid grass diagnostic read");
    std::vector<std::uint32_t> result(count);read(references,count*4,result.data());diagnosticReadBytes+=count*4;return result;
}
GrassAllocationCompute::~GrassAllocationCompute() {if(shader_) glDeleteProgram(shader_->id);}
std::unique_ptr<GrassAllocationBuffers> GrassAllocationCompute::generate(const GrassMetadataBuffers& metadata,
    const config::FoliageConfig& settings) {
    settings.validate();
    if(!limits_.unavailable.empty()) throw std::runtime_error(limits_.unavailable);
    const auto n=metadata.triangles,g=(n+63)/64;
    if(!n || n>std::numeric_limits<std::uint32_t>::max()/9 || !metadata.descriptors || !metadata.parameters ||
        !glIsBuffer(metadata.descriptors) || !glIsBuffer(metadata.parameters) ||
        metadata.generation.backend!=TerrainBackend::Compute || !metadata.generation.field || !metadata.generation.topology ||
        metadata.generation.fieldVersion!=PlanetField::version || metadata.generation.topologyVersion!=1)
        throw std::invalid_argument("Invalid grass allocation source generation");
    if(!limits_.groups) throw std::runtime_error("Zero grass allocation dispatch capacity");
    const std::array<std::uint64_t,7> bytes{n*64,160,224,g*96,n*4,n*8,g*68};
    for(auto b:bytes) if(b>limits_.blockBytes || b>std::uint64_t(std::numeric_limits<GLsizeiptr>::max()))
        throw std::runtime_error("Grass allocation exceeds SSBO block limit");
    const auto budget=std::min<std::uint64_t>(settings.max_blades,limits_.blockBytes/128);
    if(!budget) throw std::runtime_error("Insufficient grass placement capacity");
    GLint invocationLimit=0,localLimit=0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,&invocationLimit);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,0,&localLimit);
    if(invocationLimit<128 || localLimit<128) throw std::runtime_error("Insufficient grass placement work-group limit");
    if(!shader_) shader_=std::make_unique<Shader>("shaders/foliage/planning/allocation.comp");
    GLint program=0,storage=0;std::array<GLint,7> bindings{};std::array<GLint64,7> starts{},sizes{};
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&storage);
    for(int i=0;i<7;++i) {glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,i,&bindings[i]);
        glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,i,&starts[i]);glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,i,&sizes[i]);}
    struct Restore {
        GLint program,storage;std::array<GLint,7> bindings;std::array<GLint64,7> starts,sizes;
        ~Restore() {for(int i=0;i<7;++i) {
            if(bindings[i] && sizes[i]>0) glBindBufferRange(GL_SHADER_STORAGE_BUFFER,i,bindings[i],starts[i],sizes[i]);
            else glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,bindings[i]);}
            glBindBuffer(GL_SHADER_STORAGE_BUFFER,storage);glUseProgram(program);}
    } restore{program,storage,bindings,starts,sizes};
    for(const auto [b,needed]:{std::pair{metadata.descriptors,bytes[0]},std::pair{metadata.parameters,bytes[1]}}) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER,b);GLint64 actual=0;glGetBufferParameteri64v(GL_SHADER_STORAGE_BUFFER,GL_BUFFER_SIZE,&actual);
        if(actual<0 || std::uint64_t(actual)<needed) throw std::invalid_argument("Truncated grass allocation source");
    }
    auto result=std::make_unique<GrassAllocationBuffers>();result->triangles=n;result->generation=metadata.generation;result->planningEye=metadata.planningEye;
    GrassAllocationSummary initial;initial.control={std::uint32_t(budget),std::bit_floor(std::uint32_t(settings.max_candidates_per_triangle)),0,0xffffffffu};
    // The first probe reads requested density from the resident parameter pack.
    initial.status[0]=0xffffffffu;
    glGenBuffers(1,&result->state);glGenBuffers(1,&result->groups);glGenBuffers(1,&result->references);
    glGenBuffers(1,&result->ranks);glGenBuffers(1,&result->offsets);
    const std::array<GLuint,7> buffers{metadata.descriptors,metadata.parameters,result->state,result->groups,result->references,result->ranks,result->offsets};
    for(int i=2;i<7;++i) {glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffers[i]);glBufferData(GL_SHADER_STORAGE_BUFFER,bytes[i],i==2?&initial:nullptr,GL_STATIC_DRAW);result->workingBytes+=bytes[i];}
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Grass allocation buffer allocation failed");
    for(int i=0;i<7;++i) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,buffers[i]);
    shader_->use();glUniform1ui(glGetUniformLocation(shader_->id,"uGroups"),g);result->inputBytes=224+4;
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    const auto dispatch=[&](unsigned phase,bool controller=false) {
        const auto chunk=std::min<std::uint64_t>(limits_.groups,1024);
        for(std::uint64_t first=0;first<(controller?1:g);first+=chunk) {
            glUniform1ui(glGetUniformLocation(shader_->id,"uPhase"),phase);
            glUniform1ui(glGetUniformLocation(shader_->id,"uFirstGroup"),first);
            glUniform1ui(glGetUniformLocation(shader_->id,"uEndGroup"),std::min(g,first+chunk));
            glDispatchCompute(controller?1:std::min(chunk,g-first),1,1);++result->dispatches;result->inputBytes+=12;
        }
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    };
    dispatch(0);dispatch(1,true);
    for(int i=0;i<32;++i) {dispatch(2);dispatch(3,true);}
    dispatch(4);dispatch(5,true);
    for(int i=0;i<32;++i) {dispatch(6);dispatch(7,true);}
    dispatch(8);dispatch(9,true);dispatch(10);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
    result->fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    if(!result->fence || glGetError()!=GL_NO_ERROR) throw std::runtime_error("Grass allocation submission failed");
    return result;
}
}
