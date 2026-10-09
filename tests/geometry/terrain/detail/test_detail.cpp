#include "config/Config.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/character/SurfaceContact.h"
#include <gtest/gtest.h>
#include <map>
#include <limits>
#include <numbers>
using namespace rendering;
namespace {
config::PlanetConfig::TerrainLod detailLod() {
    config::PlanetConfig::TerrainLod l;
    l.max_edge_segments=l.steep_edge_segments=8;l.medium_edge_segments=4;
    l.max_triangle_budget=30000;l.shoreline_edge_m=0;
    l.relief_sinking=true;l.geometric_error_m=.05;
    l.local_detail_radius_m=.5;l.local_edge_m=.05;l.local_error_m=.01;
    l.local_transition_m=.5;return l;
}
config::PlanetConfig::SurfaceNoiseFunction microNoise() {
    config::PlanetConfig::SurfaceNoiseFunction n;
    n.amplitude_m=.01;n.wavelength_m=.25;n.octaves=1;n.seed=-71;return n;
}
}
TEST(TerrainDetail, PhysicalNoiseUsesMetersAndProtectsLatticePrecision) {
    auto n=microNoise();
    const PlanetField km({n},1,1000),meters({n},1000,1);
    EXPECT_DOUBLE_EQ(km.parameters().noise[0].frequency,4000);
    EXPECT_DOUBLE_EQ(km.parameters().gradient[2],.0125);
    for(auto r:{glm::dvec3(0,0,1),glm::dvec3(0,0,-1),glm::normalize(glm::dvec3(-1,-2,.8))}) {
        EXPECT_DOUBLE_EQ(km.heightAt(r)*1000,meters.heightAt(r));
        const auto a=km.sample(r,km.heightAt(r)),b=meters.sample(r,meters.heightAt(r));
        EXPECT_EQ(a.normal,b.normal);EXPECT_EQ(a.position,b.position);
    }
    n.wavelength_m=.01;n.octaves=6;n.lacunarity=4;
    EXPECT_THROW(PlanetField({n},100,1000),std::invalid_argument);
}
TEST(TerrainDetail, ConfigurationRejectsAmbiguousNoiseAndInvalidLocalTargets) {
    config::Config noise(nlohmann::json{{"amplitude_m",.01},{"wavelength_m",.25},{"octaves",1}});
    EXPECT_DOUBLE_EQ(config::PlanetConfig::SurfaceNoiseFunction(noise,3).wavelength_m,.25);
    config::Config ambiguous(nlohmann::json{{"frequency",8},{"wavelength_m",.25}});
    EXPECT_THROW(config::PlanetConfig::SurfaceNoiseFunction(ambiguous,3),std::invalid_argument);
    auto l=detailLod();EXPECT_NO_THROW(l.validate());
    EXPECT_DOUBLE_EQ(l.rebuildDistanceMeters(),.15);
    l.relief_sinking=false;EXPECT_THROW(l.validate(),std::invalid_argument);
    l=detailLod();l.local_detail_radius_m=std::numeric_limits<double>::infinity();
    EXPECT_THROW(l.validate(),std::invalid_argument);
    l=detailLod();l.local_edge_m=0;EXPECT_THROW(l.validate(),std::invalid_argument);
    auto n=microNoise();n.wavelength_m=-.1;EXPECT_THROW(n.validate(),std::invalid_argument);
    config::Config parsed(nlohmann::json{{"relief_sinking",true},{"geometric_error_m",.05},
        {"local_detail_radius_m",.5},{"local_edge_m",.025},{"local_error_m",.007},{"local_transition_m",1.0}});
    const config::PlanetConfig::TerrainLod settings(parsed);
    EXPECT_DOUBLE_EQ(settings.local_detail_radius_m,.5);EXPECT_DOUBLE_EQ(settings.local_edge_m,.025);
    EXPECT_DOUBLE_EQ(settings.local_error_m,.007);EXPECT_DOUBLE_EQ(settings.local_transition_m,1);
}
TEST(TerrainDetail, MicroReliefNormalsAgreeWithIndependentSmallCentralDifferences) {
    const PlanetField field({microNoise()},1,1000);
    for(int i=0;i<23;++i) {
        const auto r=glm::normalize(glm::dvec3(.137+i*.017,-.53,1));
        const auto a=glm::normalize(glm::cross(glm::dvec3(0,1,0),r)),b=glm::cross(r,a);
        const auto slope=[&](const glm::dvec3& tangent) {
            const double step=.001;
            return (field.heightAt(glm::normalize(r+tangent*(step/1000)))-
                field.heightAt(glm::normalize(r-tangent*(step/1000))))*1000/(2*step);
        };
        const auto oracle=glm::normalize(r-a*slope(a)-b*slope(b));
        const auto normal=field.sample(r,field.heightAt(r)).normal;
        EXPECT_LE(std::acos(std::clamp(glm::dot(oracle,normal),-1.0,1.0)),.02);
    }
}
TEST(TerrainDetail, CentimeterMeshIsClosedAtEquatorAndPoleAndHitsIndependentProbes) {
    const auto l=detailLod();const TerrainSurface s({microNoise()},l,1,1000);
    for(const auto radial:{glm::dvec3(1,0,0),glm::dvec3(0,0,1)}) {
        const auto t=s.buildTopologyForEye(radial*1.002,{});
        EXPECT_EQ(t.generation.topologyVersion,3u);EXPECT_FALSE(t.localDetailLimited);
        EXPECT_GT(t.localDetailTriangles,0);EXPECT_LE(t.localMaxEdgeMeters,l.local_edge_m);
        EXPECT_LE(t.localRemainingErrorRatio,1);EXPECT_LE(t.triangleCount(),l.max_triangle_budget);
        EXPECT_EQ(s.buildTopologyForEye(radial*1.002,{}).generation,t.generation);
        std::map<std::pair<unsigned,unsigned>,int> edges;
        for(std::size_t i=0;i<t.indices.size();i+=3)
            for(int j=0;j<3;++j) ++edges[std::minmax(t.indices[i+j],t.indices[i+(j+1)%3])];
        for(const auto& [edge,count]:edges) ASSERT_EQ(count,2);
        EXPECT_EQ(static_cast<long long>(t.samples.size())-edges.size()+t.triangleCount(),2);
        const auto mesh=s.evaluateTopology(t);
        SurfaceContact contact;contact.bind(mesh.vertices,mesh.indices,1,1000);
        const GroundQuery missing=[](const auto&) -> GroundContact {throw std::logic_error("Missing triangle");};
        const auto east=glm::normalize(glm::cross(radial,glm::dvec3(0,1,0)));
        const auto north=glm::cross(radial,east);
        // Fixed off-center rings, independent of the planner's midpoint probes.
        for(int ring=1;ring<=4;++ring) for(int p=0;p<19;++p) {
            const double angle=(p+.317)*2*std::numbers::pi/19;
            const auto r=glm::normalize(radial+(east*std::cos(angle)+north*std::sin(angle))*(ring*.103/1000));
            const double selected=s.field().heightAt(r,t.surfacePolicy)*1000;
            EXPECT_DOUBLE_EQ(selected,s.field().heightAt(r)*1000); // Unsunk, fully resolved core.
            EXPECT_LE(std::abs(glm::length(contact.sample(r,missing).position)-1000-selected),l.local_error_m);
        }
        // Check physical edges in the actual float render mesh, including slope.
        for(std::size_t i=0;i<mesh.indices.size();i+=3) {
            std::array<glm::dvec3,3> p;
            bool inside=false;
            for(int j=0;j<3;++j) {
                const auto offset=mesh.indices[i+j]*9;
                p[j]={mesh.vertices[offset],mesh.vertices[offset+1],mesh.vertices[offset+2]};
                inside|=glm::length(glm::normalize(p[j])-radial)*1000<l.local_detail_radius_m;
            }
            ASSERT_GT(glm::dot(glm::cross(p[1]-p[0],p[2]-p[0]),p[0]+p[1]+p[2]),0);
            if(inside) for(int j=0;j<3;++j)
                EXPECT_LE(glm::length(p[j]-p[(j+1)%3])*1000,l.local_edge_m+.0002);
        }
    }
}
TEST(TerrainDetail, InfeasibleDetailStaysBoundedAndReportsItsRemainingError) {
    auto l=detailLod();l.max_triangle_budget=10000;l.local_detail_radius_m=5;l.local_edge_m=.025;
    const TerrainSurface s({microNoise()},l,1,1000);
    const auto t=s.buildTopologyForEye({1.002,0,0},{});
    EXPECT_EQ(t.triangleCount(),10000);EXPECT_TRUE(t.errorBudgetLimited);EXPECT_TRUE(t.localDetailLimited);
    EXPECT_GT(t.localMaxEdgeMeters,l.local_edge_m);
    EXPECT_EQ(s.buildTopologyForEye({1.002,0,0},{}).generation,t.generation);
    EXPECT_TRUE(supportedTerrainTopology(1));EXPECT_TRUE(supportedTerrainTopology(2));
    EXPECT_TRUE(supportedTerrainTopology(3));EXPECT_FALSE(supportedTerrainTopology(4));
    auto bad=t;bad.surfacePolicy.spacing[7]=0;
    EXPECT_THROW(s.evaluateTopology(bad),std::invalid_argument);
}
