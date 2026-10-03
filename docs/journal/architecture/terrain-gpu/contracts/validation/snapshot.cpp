#include "rendering/geometry/Terrain.h"
#include "config/Config.h"
#include <fstream>
#include <bit>
#include <iostream>
#include <iomanip>
using namespace rendering;
std::uint64_t hashBytes(std::uint64_t h, std::uint64_t v,int n) { for(int i=0;i<n;++i) {h^=(v>>(8*i))&255;h*=1099511628211ull;}return h; }
std::uint64_t meshHash(const TerrainGeometry& g) {
 auto h=14695981039346656037ull;
 for(float v:g.vertices)h=hashBytes(h,std::bit_cast<std::uint32_t>(v),4);
 for(unsigned v:g.indices)h=hashBytes(h,v,4);
 for(float v:g.lodSinkMeters)h=hashBytes(h,std::bit_cast<std::uint32_t>(v),4);
 return h;
}
int main() {
 config::PlanetConfig::TerrainLod lod;lod.max_edge_segments=lod.steep_edge_segments=8;lod.medium_edge_segments=4;lod.max_triangle_budget=10000;lod.shoreline_edge_m=8;lod.shoreline_distance_m=300;
 config::PlanetConfig::SurfaceNoiseFunction n;n.seed=-71;n.frequency=8;n.octaves=3;n.amplitude_m=8;n.type="ridged_fbm";
 for(int kind=0;kind<3;++kind) {
  TerrainSurface t(kind==0?std::vector<config::PlanetConfig::SurfaceNoiseFunction>{}:std::vector{n},lod,kind==2?.27:1,1000,{},kind==0?std::optional<double>(0):std::nullopt);
  for(auto eye:{glm::dvec3(1.002,0,0),glm::dvec3(0,0,1.002),glm::dvec3(0,0,-1.002),glm::dvec3(4,0,0)}) {
   eye*=kind==2?.27:1;auto g=t.buildGeometryForEye(eye,{0,0,0});
   std::cout<<kind<<" "<<eye.x<<" "<<eye.y<<" "<<eye.z<<" "<<g.triangleCount()<<" "<<std::hex<<meshHash(g)<<std::dec<<"\n";
  }
  auto g=t.buildGeometry(3);std::cout<<kind<<" uniform "<<g.triangleCount()<<" "<<std::hex<<meshHash(g)<<std::dec<<"\n";
 }
 const config::ScenarioConfig scene(config::Config::load("configs/scenarios/solar_system.json"));
 const auto& p=scene.planets[0];TerrainSurface t(p.surface_noise,p.terrain_lod,p.radius,scene.metersPerWorldUnit(),p.terrain_landscape,p.water.level_m,p.terrain_material);
 for(auto eye:{glm::dvec3(1.012,0,0),glm::dvec3(0,0,1.002)}) {auto g=t.buildGeometryForEye(eye,{0,0,0});std::cout<<"production "<<eye.x<<" "<<eye.z<<" "<<g.triangleCount()<<" "<<std::hex<<meshHash(g)<<std::dec<<"\n";}
}
