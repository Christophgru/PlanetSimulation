#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include "rendering/foliage/procedural/GrassPlan.h"
#include <gtest/gtest.h>
#include <limits>
#include <map>

namespace {
struct Fixture {
    config::PlanetConfig planet;
    std::vector<float> vertices;
    std::vector<unsigned> indices;
    Fixture() {
        planet.radius=1; planet.color={.2,.6,.1}; planet.foliage.enabled=true;
        planet.foliage.draw_distance_m=100;
        for (int x=0;x<20;++x) for (int y=0;y<10;++y) {
            const unsigned start=vertices.size()/9;
            for (auto p:{glm::vec3(.2+x*.02,y*.02,1),glm::vec3(.22+x*.02,y*.02,1),glm::vec3(.2+x*.02,.02+y*.02,1)}) {
                for (const auto v:{p,glm::vec3(0,0,1),glm::vec3(1)})
                    for (int c=0;c<3;++c) vertices.push_back(v[c]);
            }
            indices.insert(indices.end(),{start,start+1,start+2});
        }
    }
    rendering::GrassPlan plan(glm::dvec3 eye={0,0,1.02}) const {
        return rendering::planGrass(vertices,indices,planet,100,eye);
    }
};
}

TEST(ProceduralGrassPlan, EveryCandidateAndPatchIsCoveredByAHardSubmittedWorkBudget) {
    Fixture f;
    for (int budget:{1,17,200,400,1000,80000}) {
        f.planet.foliage.max_blades=budget;
        const auto plan=f.plan();
        EXPECT_GT(plan.candidates,0); EXPECT_LE(plan.candidates,std::size_t(budget));
        std::size_t patches=0,count=0;
        for (int level=0;level<int(rendering::grassCandidateSlots.size());++level) {
            const auto& batch=plan.batches[level]; EXPECT_EQ(batch.first,patches);
            for (std::size_t i=0;i<batch.count;++i) {
                const auto& patch=plan.patches[batch.first+i];
                EXPECT_GE(patch.expectedCandidates,0); EXPECT_LE(patch.expectedCandidates,rendering::grassCandidateSlots[level]);
            }
            patches+=batch.count; count+=batch.count*rendering::grassCandidateSlots[level];
        }
        EXPECT_EQ(patches,plan.patches.size()); EXPECT_EQ(count,plan.candidates);
    }
}

TEST(ProceduralGrassPlan, SameMeshHasStableSeedsAndCornersAcrossSmallCameraMovement) {
    Fixture f; const auto first=f.plan(), moved=f.plan({.001,0,1.02});
    std::map<unsigned,rendering::ProceduralGrassPatch> saved;
    for (const auto& patch:first.patches) saved.emplace(patch.triangle,patch);
    ASSERT_EQ(moved.patches.size(),first.patches.size());
    for (const auto& patch:moved.patches) {
        ASSERT_TRUE(saved.contains(patch.triangle));
        EXPECT_EQ(patch.a,saved.at(patch.triangle).a); EXPECT_EQ(patch.b,saved.at(patch.triangle).b);
        EXPECT_EQ(patch.c,saved.at(patch.triangle).c);
    }
    ++f.planet.foliage.seed; EXPECT_EQ(f.plan().patches.front().triangle,first.patches.front().triangle);
}

TEST(ProceduralGrassPlan, DisabledGrayEmptyAndDistantScenesDoNotAllocateCandidates) {
    Fixture f;
    f.planet.foliage.enabled=false; EXPECT_TRUE(f.plan().patches.empty());
    f.planet.foliage.enabled=true; f.planet.color={.5,.5,.5}; EXPECT_TRUE(f.plan().patches.empty());
    f.planet.color={.2,.6,.1}; EXPECT_TRUE(f.plan({0,0,10}).patches.empty());
    f.vertices.clear(); f.indices.clear(); EXPECT_TRUE(f.plan().patches.empty());
}

TEST(ProceduralGrassPlan, PlacementControlsParseAndValidateTogether) {
    const config::FoliageConfig parsed(config::Config{nlohmann::json{
        {"enabled",false},{"rebuild_distance_fraction",.2},{"gaussian_sigma_fraction",.4},
        {"budget_fraction",.7},{"max_candidates_per_triangle",512},{"green_ratio",1.3},
        {"water_clearance_m",.3},{"root_offset_m",.01},{"height_multiplier_min",.5},
        {"height_multiplier_max",1.2},{"lean_min",.2},{"lean_max",.5}}});
    EXPECT_FALSE(parsed.enabled);
    EXPECT_EQ(parsed.rebuild_distance_fraction,.2); EXPECT_EQ(parsed.gaussian_sigma_fraction,.4);
    EXPECT_EQ(parsed.budget_fraction,.7); EXPECT_EQ(parsed.max_candidates_per_triangle,512);
    EXPECT_EQ(parsed.green_ratio,1.3); EXPECT_EQ(parsed.water_clearance_m,.3); EXPECT_EQ(parsed.root_offset_m,.01);
    EXPECT_EQ(parsed.height_multiplier_min,.5); EXPECT_EQ(parsed.height_multiplier_max,1.2);
    EXPECT_EQ(parsed.lean_min,.2); EXPECT_EQ(parsed.lean_max,.5);
    for (auto member:{&config::FoliageConfig::rebuild_distance_fraction,&config::FoliageConfig::gaussian_sigma_fraction,
        &config::FoliageConfig::budget_fraction,&config::FoliageConfig::green_ratio,&config::FoliageConfig::water_clearance_m,
        &config::FoliageConfig::root_offset_m,&config::FoliageConfig::height_multiplier_min,&config::FoliageConfig::height_multiplier_max,
        &config::FoliageConfig::lean_min,&config::FoliageConfig::lean_max}) {
        auto bad=parsed; bad.*member=std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(bad.validate(),std::invalid_argument);
    }
    auto bad=parsed; bad.height_multiplier_max=.1; EXPECT_THROW(bad.validate(),std::invalid_argument);
    bad=parsed; bad.lean_max=.1; EXPECT_THROW(bad.validate(),std::invalid_argument);
    bad=parsed; bad.max_candidates_per_triangle=0; EXPECT_THROW(bad.validate(),std::invalid_argument);
}

TEST(ProceduralGrassPlan, DetailedDescriptorsRespectHardBudgetsAndRetainCornersAndSeeds) {
    Fixture f;
    f.planet.foliage.draw_distance_m=60;
    for (int budget:{1,17,200,4096,100000}) {
        f.planet.foliage.max_blades=budget;
        const auto plan=rendering::planGrass(f.vertices,f.indices,f.planet,100,{0,0,1.02});
        EXPECT_LE(plan.candidates,budget); EXPECT_GT(plan.candidates,0);
        std::size_t count=0;
        for (std::size_t level=0;level<rendering::grassCandidateSlots.size();++level) {
            const auto& batch=plan.batches[level];
            count+=batch.count*rendering::grassCandidateSlots[level];
            for (std::size_t i=batch.first;i<batch.first+batch.count;++i)
                EXPECT_LE(plan.patches[i].expectedCandidates,rendering::grassCandidateSlots[level]);
        }
        EXPECT_EQ(count,plan.candidates);
    }
    f.planet.foliage.enabled=false;
    EXPECT_TRUE(rendering::planGrass(f.vertices,f.indices,f.planet,100,{0,0,1.02}).patches.empty());
}

TEST(ProceduralGrassPlan, WindNoiseConfigValidatesScaleSeedAndSpeed) {
    const config::FoliageConfig c(config::Config{nlohmann::json{{"wind_noise",{
        {"gust_frequency",.2},{"direction_frequency",.05},{"flutter_frequency",1.4},
        {"speed_multiplier",2},{"seed",123}}}}});
    EXPECT_EQ(c.wind_noise.gust_frequency,.2); EXPECT_EQ(c.wind_noise.direction_frequency,.05);
    EXPECT_EQ(c.wind_noise.flutter_frequency,1.4); EXPECT_EQ(c.wind_noise.speed_multiplier,2);
    EXPECT_EQ(c.wind_noise.seed,123);
    for (double invalid:{0.0,-1.0,101.0,std::numeric_limits<double>::infinity()}) {
        auto bad=c; bad.wind_noise.gust_frequency=invalid;
        EXPECT_THROW(bad.validate(),std::invalid_argument);
        bad=c; bad.wind_noise.direction_frequency=invalid;
        EXPECT_THROW(bad.validate(),std::invalid_argument);
        bad=c; bad.wind_noise.flutter_frequency=invalid;
        EXPECT_THROW(bad.validate(),std::invalid_argument);
    }
    auto bad=c; bad.wind_noise.speed_multiplier=-1; EXPECT_THROW(bad.validate(),std::invalid_argument);
    EXPECT_THROW(config::FoliageConfig(config::Config{nlohmann::json{{"wind_noise",3}}}),std::invalid_argument);
}
