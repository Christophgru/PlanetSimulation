#include "config/Config.h"
#include "rendering/geometry/Terrain.h"
#include <chrono>
#include <iostream>
#include <cmath>
#include <ctime>
int main(int argc,char** argv) {
    if(argc>2) {std::cerr << "Usage: terrain_detail_benchmark [scene.json]\n";return 2;}
    std::cout << "latitude_deg,streaming,step,wall_ms,thread_cpu_ms,triangles,height_evaluations,max_core_edge_m,core_limited,parent_reused,input_bytes\n";
    const config::ScenarioConfig scene(config::Config::load(argc==2?argv[1]:"configs/scenarios/solar_system.json"));
    const auto& p=scene.planets[0];const double meters=scene.metersPerWorldUnit();
    for(const auto pose:{glm::dvec2(-17.23072754436555,79.39197457588234),glm::dvec2(-18.726025155509635,113.23762488691663)}) {
        const double lat=pose.x*std::acos(-1.)/180,lon=pose.y*std::acos(-1.)/180;
        const glm::dvec3 radial(std::cos(lat)*std::cos(lon),std::cos(lat)*std::sin(lon),std::sin(lat));
        const auto east=glm::normalize(glm::cross(glm::dvec3(0,0,1),radial));
        for(const bool streaming:{false,true}) {
            auto l=p.terrain_lod;l.local_detail_radius_m=.5;l.local_edge_m=.05;l.local_error_m=.01;
            l.local_transition_m=.5;l.local_detail_streaming=streaming;
            const rendering::TerrainSurface surface(p.surface_noise,l,p.radius,meters,p.terrain_landscape,p.water.level_m,p.terrain_material);
            const auto eye=radial*(p.radius+surface.field().heightAt(radial)+2/meters);
            for(int step=0;step<3;++step) {
                timespec cpuStart{},cpuEnd{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpuStart);
                const auto begin=std::chrono::steady_clock::now();
                const auto t=surface.buildTopologyForEye(eye+east*(step*.15/meters),{});
                const double wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpuEnd);
                const double cpu=(cpuEnd.tv_sec-cpuStart.tv_sec)*1000.+(cpuEnd.tv_nsec-cpuStart.tv_nsec)*1e-6;
                std::cout << pose.x << ',' << streaming << ',' << step << ',' << wall << ',' << cpu << ','
                    << t.triangleCount() << ',' << t.planningQueries.evaluations << ',' << t.localMaxEdgeMeters << ','
                    << t.localDetailLimited << ',' << t.parentReused << ',' << t.topologyInputBytes << '\n';
            }
        }
    }
}
