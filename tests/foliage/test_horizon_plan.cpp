#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include "rendering/foliage/horizon/HorizonGrassPlan.h"
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
        planet.foliage.draw_distance_m=10; planet.foliage.far_distance_m=100;
        for (int x=0;x<20;++x) for (int y=0;y<10;++y) {
            const unsigned start=vertices.size()/9;
            for (auto p:{glm::vec3(.2+x*.02,y*.02,1),glm::vec3(.22+x*.02,y*.02,1),glm::vec3(.2+x*.02,.02+y*.02,1)}) {
                for (const auto v:{p,glm::vec3(0,0,1),glm::vec3(1)})
                    for (int c=0;c<3;++c) vertices.push_back(v[c]);
            }
            indices.insert(indices.end(),{start,start+1,start+2});
        }
    }
    rendering::HorizonGrassPlan plan(glm::dvec3 eye={0,0,1.02}) const {
        return rendering::planHorizonGrass(vertices,indices,planet,100,eye);
    }
};
}

TEST(HorizonPlan, SettingsParseAndRejectUnboundedOrConflictingInputs) {
    const config::FoliageConfig parsed(config::Config{nlohmann::json{{"horizon_enabled",false},
        {"far_distance_m",500},{"far_density_per_m2",.7},{"far_max_instances",1000}}});
    EXPECT_FALSE(parsed.horizon_enabled); EXPECT_EQ(parsed.far_distance_m,500);
    EXPECT_EQ(parsed.far_density_per_m2,.7); EXPECT_EQ(parsed.far_max_instances,1000);
    for (double invalid:{-1.0,20.0,20001.0,std::numeric_limits<double>::infinity()}) {
        auto bad=parsed; bad.far_distance_m=invalid; EXPECT_THROW(bad.validate(),std::invalid_argument);
    }
    auto bad=parsed; bad.far_density_per_m2=0; EXPECT_THROW(bad.validate(),std::invalid_argument);
    bad=parsed; bad.far_max_instances=250001; EXPECT_THROW(bad.validate(),std::invalid_argument);
}

TEST(HorizonPlan, HorizonIncludesEyeHeightAndReliefAndSupportsAnOverride) {
    Fixture f; f.planet.foliage.far_distance_m=0;
    const auto distance=[&](double altitude) {
        return rendering::horizonGrassDistance(f.planet,100,{0,0,1+altitude/100});
    };
    const double low=distance(2), high=distance(10);
    EXPECT_GT(low,f.planet.foliage.draw_distance_m); EXPECT_GT(high,low);
    config::PlanetConfig::SurfaceNoiseFunction noise; noise.amplitude_m=10;
    f.planet.surface_noise={noise}; EXPECT_GT(distance(2),low);
    f.planet.foliage.far_distance_m=150; EXPECT_EQ(distance(2),150);
    EXPECT_THROW(rendering::horizonGrassDistance(f.planet,0,{0,0,1}),std::invalid_argument);
    EXPECT_THROW(rendering::horizonGrassDistance(f.planet,100,{0,0,0}),std::invalid_argument);
}

TEST(HorizonPlan, EveryCandidateAndPatchIsCoveredByAHardSubmittedWorkBudget) {
    Fixture f;
    for (int budget:{1,17,200,400,1000,80000}) {
        f.planet.foliage.far_max_instances=budget;
        const auto plan=f.plan();
        EXPECT_GT(plan.candidates,0); EXPECT_LE(plan.candidates,std::size_t(budget));
        std::size_t patches=0,count=0;
        for (int level=0;level<8;++level) {
            const auto& batch=plan.batches[level]; EXPECT_EQ(batch.first,patches);
            for (std::size_t i=0;i<batch.count;++i) {
                const auto& patch=plan.patches[batch.first+i];
                EXPECT_GE(patch.expectedCandidates,0); EXPECT_LE(patch.expectedCandidates,rendering::horizonGrassSlots[level]);
            }
            patches+=batch.count; count+=batch.count*rendering::horizonGrassSlots[level];
        }
        EXPECT_EQ(patches,plan.patches.size()); EXPECT_EQ(count,plan.candidates);
    }
}

TEST(HorizonPlan, SameMeshHasStableSeedsAndCornersAcrossSmallCameraMovement) {
    Fixture f; const auto first=f.plan(), moved=f.plan({.001,0,1.02});
    std::map<unsigned,rendering::HorizonGrassPatch> saved;
    for (const auto& patch:first.patches) saved.emplace(patch.seed,patch);
    ASSERT_EQ(moved.patches.size(),first.patches.size());
    for (const auto& patch:moved.patches) {
        ASSERT_TRUE(saved.contains(patch.seed));
        EXPECT_EQ(patch.a,saved.at(patch.seed).a); EXPECT_EQ(patch.b,saved.at(patch.seed).b);
        EXPECT_EQ(patch.c,saved.at(patch.seed).c);
    }
    ++f.planet.foliage.seed; EXPECT_NE(f.plan().patches.front().seed,first.patches.front().seed);
}

TEST(HorizonPlan, DisabledGrayEmptyAndDistantScenesDoNotAllocateCandidates) {
    Fixture f;
    f.planet.foliage.horizon_enabled=false; EXPECT_TRUE(f.plan().patches.empty());
    f.planet.foliage.horizon_enabled=true; f.planet.foliage.enabled=false; EXPECT_TRUE(f.plan().patches.empty());
    f.planet.foliage.enabled=true; f.planet.color={.5,.5,.5}; EXPECT_TRUE(f.plan().patches.empty());
    f.planet.color={.2,.6,.1}; EXPECT_TRUE(f.plan({0,0,10}).patches.empty());
    f.vertices.clear(); f.indices.clear(); EXPECT_TRUE(f.plan().patches.empty());
}

TEST(HorizonPlan, RoundedBudgetMatchesIndependentLinearScanForEverySlotCap) {
    Fixture f;
    for (int slots:{1,2,8,32,128}) for (int budget:{17,200,401,1000}) {
        f.planet.foliage.far_max_candidates_per_patch=slots;
        f.planet.foliage.far_max_instances=budget;
        const auto plan=f.plan();
        std::vector<double> areas;
        double total=0;
        for (const auto& patch:plan.patches) {
            const double area=.5*glm::length(glm::cross(glm::dvec3(patch.b)-glm::dvec3(patch.a),
                glm::dvec3(patch.c)-glm::dvec3(patch.a)))*10000;
            areas.push_back(area); total+=area;
        }
        ASSERT_FALSE(areas.empty());
        const auto rounded=[slots](double expected) {
            int n=1; while (n<slots && n<expected) n*=2; return n;
        };
        const auto submitted=[&](double density) {
            std::size_t n=0; for (double area:areas) n+=rounded(area*density); return n;
        };
        double low=0,high=f.planet.foliage.far_density_per_m2;
        if (submitted(high)>std::size_t(budget)) {
            high=std::min(high,budget/total);
            for (int i=0;i<32;++i) {
                const double mid=(low+high)*.5;
                if (submitted(mid)<=std::size_t(budget)) low=mid; else high=mid;
            }
            high=low;
        }
        EXPECT_EQ(plan.candidates,submitted(high));
        for (std::size_t i=0;i<areas.size();++i)
            EXPECT_NEAR(plan.patches[i].expectedCandidates,std::min(areas[i]*high,double(slots)),1e-5);
        for (int level=0;level<8;++level) if (rendering::horizonGrassSlots[level]>slots)
            EXPECT_EQ(plan.batches[level].count,0);
    }
}

TEST(HorizonPlan, PlacementControlsParseAndValidateTogether) {
    const config::FoliageConfig parsed(config::Config{nlohmann::json{
        {"near_enabled",false},{"rebuild_distance_fraction",.2},{"gaussian_sigma_fraction",.4},
        {"budget_fraction",.7},{"max_candidates_per_triangle",512},{"green_ratio",1.3},
        {"water_clearance_m",.3},{"root_offset_m",.01},{"height_multiplier_min",.5},
        {"height_multiplier_max",1.2},{"lean_min",.2},{"lean_max",.5},
        {"far_rebuild_distance_m",20},{"far_max_candidates_per_patch",32},
        {"far_height_scale",2},{"far_width_scale",8},{"far_min_width_m",.4},
        {"far_fade_in_start_fraction",.3},{"far_fade_in_end_fraction",.8},
        {"far_fade_out_start_fraction",.9}}});
    EXPECT_FALSE(parsed.near_enabled);
    EXPECT_EQ(parsed.rebuild_distance_fraction,.2); EXPECT_EQ(parsed.gaussian_sigma_fraction,.4);
    EXPECT_EQ(parsed.budget_fraction,.7); EXPECT_EQ(parsed.max_candidates_per_triangle,512);
    EXPECT_EQ(parsed.green_ratio,1.3); EXPECT_EQ(parsed.water_clearance_m,.3); EXPECT_EQ(parsed.root_offset_m,.01);
    EXPECT_EQ(parsed.height_multiplier_min,.5); EXPECT_EQ(parsed.height_multiplier_max,1.2);
    EXPECT_EQ(parsed.lean_min,.2); EXPECT_EQ(parsed.lean_max,.5);
    EXPECT_EQ(parsed.far_rebuild_distance_m,20); EXPECT_EQ(parsed.far_max_candidates_per_patch,32);
    EXPECT_EQ(parsed.far_height_scale,2); EXPECT_EQ(parsed.far_width_scale,8); EXPECT_EQ(parsed.far_min_width_m,.4);
    EXPECT_EQ(parsed.far_fade_in_start_fraction,.3); EXPECT_EQ(parsed.far_fade_in_end_fraction,.8);
    EXPECT_EQ(parsed.far_fade_out_start_fraction,.9);
    for (auto member:{&config::FoliageConfig::rebuild_distance_fraction,&config::FoliageConfig::gaussian_sigma_fraction,
        &config::FoliageConfig::budget_fraction,&config::FoliageConfig::green_ratio,&config::FoliageConfig::water_clearance_m,
        &config::FoliageConfig::root_offset_m,&config::FoliageConfig::height_multiplier_min,&config::FoliageConfig::height_multiplier_max,
        &config::FoliageConfig::lean_min,&config::FoliageConfig::lean_max,&config::FoliageConfig::far_rebuild_distance_m,
        &config::FoliageConfig::far_height_scale,&config::FoliageConfig::far_width_scale,&config::FoliageConfig::far_min_width_m,
        &config::FoliageConfig::far_fade_in_start_fraction,&config::FoliageConfig::far_fade_in_end_fraction,
        &config::FoliageConfig::far_fade_out_start_fraction}) {
        auto bad=parsed; bad.*member=std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(bad.validate(),std::invalid_argument);
    }
    auto bad=parsed; bad.height_multiplier_max=.1; EXPECT_THROW(bad.validate(),std::invalid_argument);
    bad=parsed; bad.lean_max=.1; EXPECT_THROW(bad.validate(),std::invalid_argument);
    bad=parsed; bad.far_fade_in_end_fraction=.1; EXPECT_THROW(bad.validate(),std::invalid_argument);
    bad=parsed; bad.far_max_candidates_per_patch=3; EXPECT_THROW(bad.validate(),std::invalid_argument);
    bad=parsed; bad.max_candidates_per_triangle=0; EXPECT_THROW(bad.validate(),std::invalid_argument);
}
