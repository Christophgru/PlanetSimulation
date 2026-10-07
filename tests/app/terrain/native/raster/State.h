#pragma once
#include <cmath>
#include <cstring>

namespace raster {
inline json events=json::array();
inline json other={{"elements",0},{"arrays",0},{"instanced",0},{"indirect",0}};
inline json* active=nullptr;
inline float bladeTime=0;
inline bool bladeTimeSeen=false, started=false;
inline std::string phase;
inline std::uint64_t sequence=0;
inline json frameControl;
inline GLuint program=0;
inline PFNGLUSEPROGRAMPROC useProgram=nullptr;
inline PFNGLDRAWARRAYSINSTANCEDPROC instanced=nullptr;
inline PFNGLDRAWARRAYSINDIRECTPROC indirect=nullptr;

inline void begin() {
    if(started) return;
    const auto next=controls.value("phase",std::string("startup"));
    if(next!=phase) {phase=next;sequence=0;}
    const auto wind=controls.value("wind_mode",std::string("native"));
    const auto draw=controls.value("raster_mode",std::string("full"));
    if(wind!="native" && wind!="fixed" && wind!="indexed") throw std::runtime_error("Invalid private wind mode");
    if(draw!="full" && draw!="discard" && draw!="suppress") throw std::runtime_error("Invalid private raster mode");
    const double start=controls.value("wind_start_s",12.0),step=controls.value("wind_step_s",.125);
    if(!std::isfinite(start) || !std::isfinite(step) || start<0 || step<0 || start+step*sequence>1e6)
        throw std::runtime_error("Invalid private wind clock");
    frameControl={{"phase",phase},{"sequence",sequence},{"wind_mode",wind},{"raster_mode",draw},
        {"wind_start_s",start},{"wind_step_s",step},{"qualification",controls.value("qualify_raster",false)}};
    started=true;
}

inline bool suppress() {return active && active->at("raster_mode")=="suppress";}
inline void call(const char* kind,GLenum mode,GLsizei vertices,GLsizei instances,std::uintptr_t offset=0) {
    if(!active) {other[kind]=other.at(kind).get<unsigned>()+1;return;}
    if(program!=active->at("blade_program").get<GLuint>() || mode!=GL_TRIANGLE_STRIP)
        throw std::runtime_error("Private raster hook intercepted another draw");
    const bool commands=std::strcmp(kind,"indirect")==0;
    (*active)["draws"].push_back({{"kind",kind},{"mode",mode},{"vertices",commands ? json(nullptr) : json(vertices)},
        {"instances",commands ? json(nullptr) : json(instances)},{"offset_bytes",offset},{"submitted",!suppress()}});
}
}
