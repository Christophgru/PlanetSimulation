#pragma once
#include "State.h"

extern "C" void realFloat(const Shader*,const char*,float) asm("__real__ZNK6Shader8setFloatEPKcf");
extern "C" void wrappedFloat(const Shader*,const char*,float) asm("__wrap__ZNK6Shader8setFloatEPKcf");
extern "C" void wrappedFloat(const Shader* self,const char* name,float value) {
    if(observing && renderer && std::strcmp(name,"uTime")==0) {
        const auto& r=rendering::RendererRecoveryProbe::state(*renderer);
        if(self==&r.grass.shader) {raster::bladeTime=value;raster::bladeTimeSeen=true;}
        else if(raster::active) {
            (*raster::active)["placement_program"]=self->id;
            (*raster::active)["placement_time_s"]=value;
            (*raster::active)["placement_time_writes"]=raster::active->at("placement_time_writes").get<unsigned>()+1;
        }
    }
    realFloat(self,name,value);
}

namespace raster {
inline void GLAPIENTRY use(GLuint id) {program=id;useProgram(id);}
inline void GLAPIENTRY drawInstanced(GLenum mode,GLint first,GLsizei count,GLsizei instances) {
    call("instanced",mode,count,instances);
    if(!suppress()) instanced(mode,first,count,instances);
}
inline void GLAPIENTRY drawIndirect(GLenum mode,const void* offset) {
    call("indirect",mode,0,0,reinterpret_cast<std::uintptr_t>(offset));
    if(!suppress()) indirect(mode,offset);
}
inline void install(GLuint bladeProgram) {
    if(useProgram) return;
    useProgram=__glewUseProgram;instanced=__glewDrawArraysInstanced;indirect=__glewDrawArraysIndirect;
    program=bladeProgram;
    __glewUseProgram=use;__glewDrawArraysInstanced=drawInstanced;__glewDrawArraysIndirect=drawIndirect;
}
}

extern "C" void realGrass(const rendering::GrassRenderer*,std::size_t,const rendering::GrassPass*)
    asm("__real__ZNK9rendering13GrassRenderer4drawEmPKNS_9GrassPassE");
extern "C" void wrappedGrass(const rendering::GrassRenderer*,std::size_t,const rendering::GrassPass*)
    asm("__wrap__ZNK9rendering13GrassRenderer4drawEmPKNS_9GrassPassE");
extern "C" void wrappedGrass(const rendering::GrassRenderer* self,std::size_t body,const rendering::GrassPass* pass) {
    if(!observing || !pass) {realGrass(self,body,pass);return;}
    raster::begin();raster::install(self->shader.id);
    if(raster::active || !raster::bladeTimeSeen || raster::bladeTime!=pass->windTime)
        throw std::runtime_error("Private wind receipt does not match blade/placement input");
    auto copy=*pass;
    const auto wind=raster::frameControl.at("wind_mode").get<std::string>();
    if(wind!="native") {
        copy.windTime=float(raster::frameControl.at("wind_start_s").get<double>()+
            (wind=="indexed" ? raster::sequence*raster::frameControl.at("wind_step_s").get<double>() : 0));
        realFloat(&self->shader,"uTime",copy.windTime);
    }
    json event={{"body",body},{"view",pass->timingView==rendering::GpuWorkView::Main ? "main" : "reflection"},
        {"original_time_s",pass->windTime},{"original_blade_time_s",raster::bladeTime},
        {"effective_time_s",copy.windTime},{"blade_program",self->shader.id},
        {"raster_mode",raster::frameControl.at("raster_mode")},{"placement_time_writes",0},
        {"draws",json::array()}};
    raster::active=&event;
    const bool discard=raster::frameControl.at("raster_mode")=="discard";
    const bool wasDiscard=glIsEnabled(GL_RASTERIZER_DISCARD);
    if(wasDiscard) throw std::runtime_error("Unexpected pre-existing rasterizer discard");
    const bool qualify=raster::frameControl.at("qualification");
    GLuint queries[2]{};
    if(qualify) {glGenQueries(2,queries);glBeginQuery(GL_PRIMITIVES_GENERATED,queries[0]);glBeginQuery(GL_SAMPLES_PASSED,queries[1]);}
    if(discard) glEnable(GL_RASTERIZER_DISCARD);
    realGrass(self,body,&copy);
    if(discard) glDisable(GL_RASTERIZER_DISCARD);
    if(qualify) {
        glEndQuery(GL_PRIMITIVES_GENERATED);glEndQuery(GL_SAMPLES_PASSED);
        GLuint primitives=0,samples=0;
        // Synchronous diagnostic reads exist only in explicitly excluded fixture
        // qualification, never a measured production frame.
        glGetQueryObjectuiv(queries[0],GL_QUERY_RESULT,&primitives);
        glGetQueryObjectuiv(queries[1],GL_QUERY_RESULT,&samples);glDeleteQueries(2,queries);
        event["generated_primitives"]=primitives;event["passed_samples"]=samples;
        event["blocking_qualification_query_reads"]=2;
        float actual=0;glGetUniformfv(self->shader.id,glGetUniformLocation(self->shader.id,"uTime"),&actual);
        if(actual!=copy.windTime) throw std::runtime_error("Private blade wind uniform mismatch");
        event["verified_blade_time_s"]=actual;
        if(event.contains("placement_program")) {
            const GLuint id=event.at("placement_program");glGetUniformfv(id,glGetUniformLocation(id,"uTime"),&actual);
            if(actual!=copy.windTime) throw std::runtime_error("Private placement wind uniform mismatch");
            event["verified_placement_time_s"]=actual;
        }
    }
    raster::active=nullptr;raster::bladeTimeSeen=false;raster::events.push_back(std::move(event));
}

extern "C" void GLAPIENTRY glDrawElements(GLenum mode,GLsizei count,GLenum type,const void* indices) {
    static auto draw=reinterpret_cast<void(*)(GLenum,GLsizei,GLenum,const void*)>(dlsym(RTLD_NEXT,"glDrawElements"));
    if(observing) raster::other["elements"]=raster::other.at("elements").get<unsigned>()+1;
    draw(mode,count,type,indices);
}
extern "C" void GLAPIENTRY glDrawArrays(GLenum mode,GLint first,GLsizei count) {
    static auto draw=reinterpret_cast<void(*)(GLenum,GLint,GLsizei)>(dlsym(RTLD_NEXT,"glDrawArrays"));
    if(observing) raster::other["arrays"]=raster::other.at("arrays").get<unsigned>()+1;
    draw(mode,first,count);
}

template<class State> nlohmann::json rasterBenchmarkObservation(State& r,GLFWwindow* window) {
    auto result=benchmarkObservation(r,window);
    raster::begin();
    result["grass_raster"]={{"schema",1},{"control",raster::frameControl},
        {"events",std::move(raster::events)},{"other_draw_calls",raster::other},
        {"full_draw_timing_eligible",!raster::frameControl.at("qualification").get<bool>() && raster::frameControl.at("raster_mode")=="full"},
        {"timing_acceptance",false}};
    raster::events=json::array();
    raster::other={{"elements",0},{"arrays",0},{"instanced",0},{"indirect",0}};
    raster::started=false;++raster::sequence;
    return result;
}
