#include "config/ScenarioConfig.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <limits>
#include "rendering/foliage/GrassPlacement.h"

namespace {
std::vector<float> patch(glm::vec3 normal={0,0,1},glm::vec3 tint={1,1,1}) {
    std::vector<float> result;
    for (glm::vec3 p : {glm::vec3(-.1,-.1,1),glm::vec3(.1,-.1,1),
                       glm::vec3(.1,.1,1),glm::vec3(-.1,.1,1)}) {
        for (const auto v : {p,normal,tint}) for (int c=0;c<3;++c) result.push_back(v[c]);
    }
    return result;
}
const std::vector<unsigned> indices{0,1,2,0,2,3};
config::PlanetConfig planet() {
    config::PlanetConfig p;
    p.radius=1; p.color={.2,.6,.1}; p.foliage.enabled=true;
    p.foliage.draw_distance_m=10; p.foliage.max_blades=2000;
    return p;
}
}
TEST(GrassPlacement, DeterministicBoundedAndAttachedToRenderedGround) {
    const auto p=planet();
    const auto a=rendering::placeGrass(patch(),indices,p,100,{0,0,1.02});
    const auto b=rendering::placeGrass(patch(),indices,p,100,{0,0,1.02});
    ASSERT_GT(a.size(),500u); ASSERT_LE(a.size(),2000u);
    ASSERT_EQ(a.size(),b.size());
    for (std::size_t i=0;i<a.size();++i) {
        EXPECT_EQ(a[i].root,b[i].root); EXPECT_EQ(a[i].variation,b[i].variation);
        EXPECT_NEAR(a[i].root.z,1.00005,1e-6);
        EXPECT_NEAR(glm::length(a[i].up),1,1e-6);
    }
    const auto moved=rendering::placeGrass(patch(),indices,p,100,{.02,0,1.02});
    int common=0;
    for (const auto& blade:a) {
        if (glm::length(glm::dvec3(blade.root)-glm::dvec3(.02,0,1.02))*100>10) continue;
        for (const auto& other:moved) if (blade.root==other.root) { ++common; break; }
    }
    EXPECT_GT(common,500); // Moving the patch does not re-seed its overlap.
    auto tiny=p; tiny.foliage.max_blades=7;
    EXPECT_LE(rendering::placeGrass(patch(),indices,tiny,100,{0,0,1.02}).size(),7u);
}
TEST(GrassPlacement, RejectsWaterCliffsSnowDisabledAndDistantViews) {
    auto p=planet();
    EXPECT_TRUE(rendering::placeGrass(patch({1,0,0}),indices,p,100,{0,0,1.02}).empty());
    EXPECT_TRUE(rendering::placeGrass(patch({0,0,1},{3,1,6}),indices,p,100,{0,0,1.02}).empty());
    EXPECT_TRUE(rendering::placeGrass(patch(),indices,p,100,{0,0,2}).empty());
    p.water.enabled=true; p.water.level_m=5;
    EXPECT_TRUE(rendering::placeGrass(patch(),indices,p,100,{0,0,1.02}).empty());
    p.water.enabled=false; p.foliage.enabled=false;
    EXPECT_TRUE(rendering::placeGrass(patch(),indices,p,100,{0,0,1.02}).empty());
}
TEST(GrassPlacement, GaussianDensityFallsWithDistanceAndFollowsTheEye) {
    auto p=planet();
    p.foliage.max_blades=12000;
    // Subdivide a wide planar patch so the per-triangle work cap does not
    // limit the measured distribution. Three equal-area annuli distinguish
    // Gaussian density from a uniform disk with only an outer cutoff.
    std::vector<float> vertices;
    std::vector<unsigned> triangles;
    for (int y=-20;y<20;++y) for (int x=-20;x<20;++x) {
        const auto base=static_cast<unsigned>(vertices.size()/9);
        for (auto corner: {glm::dvec2(x,y),glm::dvec2(x+1,y),
                           glm::dvec2(x+1,y+1),glm::dvec2(x,y+1)}) {
            for (auto value: {float(corner.x/100),float(corner.y/100),1.0f,
                             0.0f,0.0f,1.0f,1.0f,1.0f,1.0f}) vertices.push_back(value);
        }
        triangles.insert(triangles.end(),{base,base+1,base+2,base,base+2,base+3});
    }
    for (double x: {0.0,0.05}) {
        const auto blades=rendering::placeGrass(vertices,triangles,p,100,{x,0,1.0});
        ASSERT_GT(blades.size(),1000u);
        ASSERT_LE(blades.size(),std::size_t(p.foliage.max_blades));
        std::array<int,3> annuli{};
        double meanX=0,meanY=0;
        for (const auto& blade:blades) {
            const double dx=(blade.root.x-x)*100,dy=blade.root.y*100;
            const int bin=int((dx*dx+dy*dy)/100*3);
            if (bin<3) ++annuli[bin];
            meanX+=dx; meanY+=dy;
        }
        EXPECT_GT(annuli[0],3*annuli[1]);
        EXPECT_GT(annuli[1],3*annuli[2]);
        EXPECT_NEAR(meanX/blades.size(),0,0.3);
        EXPECT_NEAR(meanY/blades.size(),0,0.3);
    }
}
TEST(GrassPlacement, BothPolesUseTheSameBodyLocalPlacement) {
    const auto p=planet();
    const auto north=rendering::placeGrass(patch(),indices,p,100,{0,0,1.02});
    auto southPatch=patch();
    for (std::size_t i=0;i<southPatch.size();i+=9) {
        southPatch[i+1]*=-1; southPatch[i+2]*=-1;
        southPatch[i+4]*=-1; southPatch[i+5]*=-1;
    }
    const auto south=rendering::placeGrass(southPatch,indices,p,100,{0,0,-1.02});
    ASSERT_EQ(north.size(),south.size());
    for (std::size_t i=0;i<north.size();++i) {
        EXPECT_EQ(north[i].root*glm::vec3(1,-1,-1),south[i].root);
        EXPECT_EQ(north[i].variation,south[i].variation);
    }
}
TEST(GrassPlacement, ShoreBiomeUsesRootHeightInsteadOfCoarseVertexTint) {
    auto p=planet();
    p.terrain_landscape.enabled=true;
    p.terrain_landscape.continent_amplitude_m=10;
    p.water.enabled=true;
    auto ground=patch({0,0,1},{3,1,6}); // Coarse snow-colored corners.
    for (std::size_t i=0;i<ground.size();i+=9) {
        ground[i]*=.2f; ground[i+1]*=.2f; ground[i+2]=1.003f;
    }
    const auto grass=rendering::placeGrass(ground,indices,p,100,{0,0,1.02});
    ASSERT_GT(grass.size(),50u);
    for (const auto& blade:grass) {
        const double height=(glm::length(glm::dvec3(blade.root))-1)*100;
        EXPECT_GT(height,.25); EXPECT_LT(height,.4);
    }
    // The same low patch at sea level must not grow grass through the water.
    for (std::size_t i=0;i<ground.size();i+=9) ground[i+2]=1.0f;
    EXPECT_TRUE(rendering::placeGrass(ground,indices,p,100,{0,0,1.02}).empty());
}
TEST(GrassConfig, ParsesAndRejectsUnboundedOrNonfiniteWork) {
    config::PlanetConfig p(config::Config{nlohmann::json::parse(R"({"foliage":{"height_m":0.8,"max_blades":1200}})")});
    EXPECT_TRUE(p.foliage.enabled); EXPECT_DOUBLE_EQ(p.foliage.height_m,.8);
    EXPECT_EQ(p.foliage.max_blades,1200);
    EXPECT_FALSE(config::PlanetConfig{}.foliage.enabled);
    EXPECT_NO_THROW(config::FoliageConfig(config::Config{nlohmann::json::parse(R"({"density_per_m2":1200.72})")}));
    EXPECT_NO_THROW(config::FoliageConfig(config::Config{nlohmann::json::parse(R"({"draw_distance_m":400})")}));
    for (const auto* raw : {R"({"max_blades":0})",R"({"max_blades":250001})",
         R"({"density_per_m2":4097})",R"({"draw_distance_m":401})",R"({"width_m":0})",
         R"({"height_m":4})",R"({"wind_strength":-1})"})
        EXPECT_THROW(config::FoliageConfig(config::Config{nlohmann::json::parse(raw)}),std::invalid_argument);
    p.foliage.height_m=std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(p.foliage.validate(),std::invalid_argument);
}
