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
TEST(GrassConfig, ParsesAndRejectsUnboundedOrNonfiniteWork) {
    config::PlanetConfig p(config::Config{nlohmann::json::parse(R"({"foliage":{"height_m":0.8,"max_blades":1200}})")});
    EXPECT_TRUE(p.foliage.enabled); EXPECT_DOUBLE_EQ(p.foliage.height_m,.8);
    EXPECT_EQ(p.foliage.max_blades,1200);
    EXPECT_FALSE(config::PlanetConfig{}.foliage.enabled);
    EXPECT_NO_THROW(config::FoliageConfig(config::Config{nlohmann::json::parse(R"({"density_per_m2":1200.72})")}));
    for (const auto* raw : {R"({"max_blades":0})",R"({"max_blades":250001})",
         R"({"density_per_m2":4097})",R"({"draw_distance_m":101})",R"({"width_m":0})",
         R"({"height_m":4})",R"({"wind_strength":-1})"})
        EXPECT_THROW(config::FoliageConfig(config::Config{nlohmann::json::parse(raw)}),std::invalid_argument);
    p.foliage.height_m=std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(p.foliage.validate(),std::invalid_argument);
}
