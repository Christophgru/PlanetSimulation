#include "config/Config.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <chrono>
#include <iostream>
#include <cmath>
#include <ctime>
int main(int argc,char** argv) {
    if(argc>3) {std::cerr << "Usage: terrain_detail_benchmark [scene.json] [cpu-trace.json]\n";return 2;}
    rendering::CpuTrace trace(argc==3?argv[2]:"");
    rendering::CpuTrace::Thread thread(&trace,"detail benchmark");
    std::cout << "latitude_deg,streaming,step,wall_ms,thread_cpu_ms,triangles,height_evaluations,max_core_edge_m,core_limited,parent_reused,input_bytes,topology_fingerprint,parent_edges,mutable_edges,reference_parent_positions,needed_parent_positions,contact_wall_ms,contact_thread_cpu_ms,contact_index_bytes,contact_candidate_fingerprint,contact_parent_triangles,contact_local_triangles,contact_shared_parent_bytes\n";
    const config::ScenarioConfig scene(config::Config::load(argc>=2?argv[1]:"configs/scenarios/solar_system.json"));
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
                // Per-vertex requirement, not an issued-invocation counter.
                std::size_t neededParents=0;
                for(const auto& m:t.morph) if(m.blend[0]>0)
                    neededParents+=m.parent[0]==m.parent[1] && m.parent[0]==m.parent[2]?1:3;
                clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpuStart);
                const auto contactStart=std::chrono::steady_clock::now();
                rendering::SparseTerrainContacts contacts(surface.field(),t);
                const double contactWall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-contactStart).count();
                clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpuEnd);
                const double contactCpu=(cpuEnd.tv_sec-cpuStart.tv_sec)*1000.+(cpuEnd.tv_nsec-cpuStart.tv_nsec)*1e-6;
                // Independent of planner timing. Retain the complete ordered
                // broad-phase result, including shared-edge/pole candidates.
                auto candidateHash=14695981039346656037ull;
                const auto checkCandidates=[&](const glm::dvec3& direction) {
                    const auto candidates=contacts.candidates(direction);
                    candidateHash=rendering::terrainHashWord(candidateHash,candidates.size());
                    for(auto id:candidates) candidateHash=rendering::terrainHashWord(candidateHash,id,4);
                };
                for(const auto direction:{glm::dvec3(1,0,0),glm::dvec3(-1,0,0),glm::dvec3(0,1,0),
                                          glm::dvec3(0,-1,0),glm::dvec3(0,0,1),glm::dvec3(0,0,-1)}) checkCandidates(direction);
                for(int i=0;i<256;++i) {
                    const double z=1-2*(i+.5)/256,r=std::sqrt(1-z*z),angle=i*2.399963229728653;
                    checkCandidates({r*std::cos(angle),r*std::sin(angle),z});
                }
                for(std::size_t i=0;i<t.samples.size();i+=211) {
                    const auto& r=t.samples[i].radial;checkCandidates({r[0],r[1],r[2]});
                }
                std::cout << pose.x << ',' << streaming << ',' << step << ',' << wall << ',' << cpu << ','
                    << t.triangleCount() << ',' << t.planningQueries.evaluations << ',' << t.localMaxEdgeMeters << ','
                    << t.localDetailLimited << ',' << t.parentReused << ',' << t.topologyInputBytes << ',' << t.generation.topology << ','
                    << t.refinementParentEdges << ',' << t.refinementMutableEdges << ',' << 3*t.morph.size() << ',' << neededParents << ','
                    << contactWall << ',' << contactCpu << ',' << contacts.stats().indexBytes << ',' << candidateHash << ','
                    << contacts.stats().parentTriangles << ',' << contacts.stats().localTriangles << ',' << contacts.stats().sharedParentBytes << '\n';
            }
        }
    }
}
