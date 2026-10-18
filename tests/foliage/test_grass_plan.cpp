#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include "rendering/foliage/procedural/GrassPlan.h"
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include "rendering/foliage/planning/GrassBudget.h"
#include "rendering/geometry/terrain/detail/TerrainMorph.h"

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

TEST(GrassBudget, MissingStaleAndOverlapMemoryAreConservative) {
    using namespace rendering;
    constexpr std::uint64_t mib=1024*1024;
    GrassBudgetController c;
    EXPECT_EQ(c.available(0,1024*mib),64*mib);
    EXPECT_EQ(c.available(240*mib,1024*mib),16*mib);
    EXPECT_EQ(c.available(300*mib,1024*mib),0u);
    GrassBudgetSignals s;s.nowNs=s.memoryNs=10000000000ull;s.freeBytes=400*mib;s.sampleReserved=100*mib;
    c.observe(s);EXPECT_EQ(c.available(100*mib,1024*mib),200*mib);
    EXPECT_EQ(c.available(150*mib,1024*mib),150*mib);
    EXPECT_EQ(c.available(150*mib,80*mib),80*mib);
    s.capBytes=180*mib;c.observe(s);EXPECT_EQ(c.available(150*mib,1024*mib),30*mib);
    s.capBytes.reset();s.nowNs+=2100000000ull;c.observe(s);
    EXPECT_EQ(c.available(100*mib,1024*mib),64*mib);
    s.freeBytes=0;s.memoryNs=s.nowNs;c.observe(s);EXPECT_EQ(c.available(0,1024*mib),0u);
}
TEST(GrassBudget, DistinctTimingSamplesCooldownAndMemoryHysteresisBoundAdaptation) {
    using namespace rendering;
    GrassBudgetController c;GrassBudgetSignals s;s.nowNs=10000000000ull;
    s.gpuNs=s.nowNs;s.gpuMs=60;s.utilization=99;s.gpuSample=1;c.observe(s);
    for(int i=0;i<20;++i) c.observe(s);EXPECT_EQ(c.scale(),1.); // Cached sample is counted once.
    for(unsigned i=2;i<=3;++i) {s.gpuSample=i;c.observe(s);}
    EXPECT_DOUBLE_EQ(c.scale(),.8);const auto revision=c.revision();
    for(unsigned i=4;i<20;++i) {s.gpuSample=i;c.observe(s);}
    EXPECT_DOUBLE_EQ(c.scale(),.8);EXPECT_EQ(c.revision(),revision);
    s.nowNs+=2100000000ull;s.gpuNs=s.nowNs;++s.gpuSample;c.observe(s);
    EXPECT_NEAR(c.scale(),.64,1e-12);
    s.nowNs+=2100000000ull;s.gpuNs=s.nowNs;s.gpuMs=10;
    for(int i=0;i<3;++i) {++s.gpuSample;c.observe(s);}
    EXPECT_NEAR(c.scale(),.704,1e-12);
    s.nowNs+=2100000000ull;s.memoryNs=s.nowNs;s.freeBytes=1000000000;c.observe(s);
    const auto stable=c.revision();s.nowNs+=2100000000ull;s.memoryNs=s.nowNs;s.freeBytes=990000000;c.observe(s);
    EXPECT_EQ(c.revision(),stable);
    s.freeBytes=700000000;c.observe(s);EXPECT_GT(c.revision(),stable);
    s.nowNs=1;c.observe(s); // A reset clock cannot bypass the cooldown via unsigned underflow.
    EXPECT_GE(c.scale(),.05);
}
TEST(GrassBudget, EffectiveReplayRoundTripsAndRejectsInvalidLimits) {
    using namespace rendering;
    config::FoliageConfig f;
    GrassFalloff p;p.enabled=true;p.capacity=10000;p.budget=7000;p.protectedMeters=f.quadDistanceMeters();
    p.sigmaMeters=3.12345678901234;p.density=f.density_per_m2;
    const auto saved=nlohmann::json::parse(p.json().dump());
    const auto q=GrassFalloff::replay(saved,f);EXPECT_TRUE(q.locked);EXPECT_EQ(q.json(),saved);
    for(const auto& [key,value]:std::vector<std::pair<std::string,nlohmann::json>>{
        {"version",2},{"capacity",-1},{"capacity",4294967297ull},{"budget",10001},
        {"protected_m",.1},{"sigma_m",-1},{"density",f.density_per_m2+1},{"near_infeasible",0}}) {
        auto bad=saved;bad[key]=value;EXPECT_THROW(GrassFalloff::replay(bad,f),std::exception)<<key;
    }
    auto signedValues=p.json();signedValues["capacity"]=10000;signedValues["budget"]=7000;
    EXPECT_NO_THROW(GrassFalloff::replay(signedValues,f));
}

TEST(GrassBudget, CpuProtectedAllocationPreservesNearDensityAndReportsPhysicalDeficits) {
    using namespace rendering;
    config::FoliageConfig f;f.max_blades=128;f.max_candidates_per_triangle=128;
    f.draw_distance_m=150;f.density_per_m2=8;
    std::vector<GrassPlanTriangle> triangles;
    for(unsigned i=0;i<103;++i) {
        GrassPlanTriangle t;t.patch.triangle=i;t.area=1;
        t.distance=t.minimumDistance=i<3?double(i+1):11.+(i-3)*.8;
        triangles.push_back(t);
    }
    GrassFalloff p;p.enabled=true;p.capacity=128;p.budget=80;
    p.protectedMeters=10;p.sigmaMeters=40;p.density=8;
    const auto normal=allocateProtectedGrass(triangles,f,p);
    p.budget=40;const auto busy=allocateProtectedGrass(triangles,f,p);
    EXPECT_EQ(normal.density,8);EXPECT_EQ(busy.density,8);
    EXPECT_FALSE(normal.falloff.nearInfeasible);EXPECT_FALSE(busy.falloff.nearInfeasible);
    EXPECT_LT(busy.falloff.sigmaMeters,normal.falloff.sigmaMeters);
    EXPECT_LE(normal.candidates,80);EXPECT_LE(busy.candidates,40);
    for(const auto* plan:{&normal,&busy}) {
        const auto batch=plan->batches[3];
        for(unsigned id=0;id<3;++id) {
            EXPECT_TRUE(std::any_of(plan->patches.begin()+batch.first,plan->patches.begin()+batch.first+batch.count,
                [id](const auto& t) {return t.triangle==id;}));
        }
        auto locked=plan->falloff;locked.locked=true;
        const auto restored=allocateProtectedGrass(triangles,f,locked);
        EXPECT_EQ(restored.candidates,plan->candidates);EXPECT_EQ(restored.falloff.json(),locked.json());
    }
    p.budget=1;const auto nearOnly=allocateProtectedGrass(triangles,f,p);
    EXPECT_EQ(nearOnly.falloff.budget,24);EXPECT_EQ(nearOnly.candidates,24);EXPECT_EQ(nearOnly.density,8);
    p.capacity=2;const auto tiny=allocateProtectedGrass(triangles,f,p);
    EXPECT_TRUE(tiny.falloff.nearInfeasible);EXPECT_LE(tiny.candidates,2);EXPECT_GT(tiny.candidates,0);
    for(const auto& t:tiny.patches) EXPECT_LT(t.triangle,3);
    p.capacity=p.budget=128;p.locked=true;p.sigmaMeters=0;p.nearInfeasible=false;
    f.max_candidates_per_triangle=2;
    EXPECT_TRUE(allocateProtectedGrass(triangles,f,p).falloff.nearInfeasible);
}

TEST(ProceduralGrassPlan, RetainedEnvelopePreservesPotentialFineAnchorsAndBoundsIndependentBakedCorners) {
    using namespace rendering;
    config::PlanetConfig planet;planet.radius=1;planet.color={1,1,1};planet.foliage.enabled=true;
    planet.foliage.draw_distance_m=100;planet.foliage.quad_distance_m=15;
    planet.foliage.density_per_m2=2;planet.foliage.max_blades=4096;
    planet.foliage.max_candidates_per_triangle=4096;planet.water.enabled=true;planet.water.level_m=0;
    const std::vector<unsigned> indices{0,1,2};
    std::size_t phases=0;
    for(const auto radial:{glm::dvec3(0,0,1),glm::normalize(glm::dvec3(.7,-.2,1))}) {
        const auto east=glm::normalize(glm::cross(radial,glm::dvec3(0,1,0)));
        const glm::dmat3 rotation(east,glm::cross(radial,east),radial);
        for(const bool collapsed:{false,true}) {
            std::array<glm::dvec3,3> fine,parent;
            TerrainMorphData source;source.generation={123,456,PlanetField::version,4,TerrainBackend::Cpu};
            source.metersPerRadius=1000;source.targets.resize(54);source.corners={2,0,1};
            const std::array<glm::dvec3,3> triangle{{{-.01,-.01,1.002},{.01,-.01,1.002},{0,.01,1.002}}};
            for(int i=0;i<3;++i) {
                fine[i]=glm::vec3(rotation*triangle[i]);
                parent[i]=rotation*(collapsed?glm::dvec3(.03,-.01,.998):triangle[i]+glm::dvec3(.03,0,-.004));
                const auto direction=glm::normalize(fine[i]);const auto sample=source.corners[i];
                for(int j=0;j<3;++j) {
                    const auto r=std::bit_cast<std::uint64_t>(direction[j]),p=std::bit_cast<std::uint64_t>(parent[i][j]);
                    source.targets[9*sample+2*j]=unsigned(r);source.targets[9*sample+2*j+1]=unsigned(r>>32);
                    source.targets[9*sample+6+j]=std::bit_cast<unsigned>(float(fine[i][j]));
                    source.targets[27+9*sample+2*j]=unsigned(p);source.targets[27+9*sample+2*j+1]=unsigned(p>>32);
                    source.targets[27+9*sample+6+j]=std::bit_cast<unsigned>(float(parent[i][j]));
                }
            }
            const auto vertices=[&](const glm::dvec3& weights) {
                std::vector<float> values;
                for(int i=0;i<3;++i) for(const auto v:{glm::vec3(fine[i]+(parent[i]-fine[i])*weights[i]),glm::vec3(radial),glm::vec3(.2,.8,.2)})
                    for(int j=0;j<3;++j) values.push_back(v[j]);
                return values;
            };
            const auto submerged=vertices(glm::dvec3(1));const auto eye=radial*1.002;
            EXPECT_EQ(planGrass(submerged,indices,planet,1000,eye).candidates,0u);
            for(const bool protectedPolicy:{false,true}) {
                GrassFalloff policy;policy.enabled=protectedPolicy;policy.capacity=policy.budget=4096;
                policy.protectedMeters=100;policy.sigmaMeters=40;policy.density=2;
                const auto baseline=planGrass(submerged,indices,planet,1000,eye,policy,&source);
                ASSERT_EQ(baseline.patches.size(),1u);ASSERT_EQ(baseline.morphBounds.size(),1u);
                ASSERT_GT(baseline.candidates,0u);EXPECT_FALSE(baseline.falloff.nearInfeasible);
                EXPECT_EQ(baseline.patches[0].a,glm::vec3(fine[0]));EXPECT_EQ(baseline.patches[0].b,glm::vec3(fine[1]));
                EXPECT_EQ(baseline.patches[0].c,glm::vec3(fine[2]));
                double largest=0;
                for(double a:{0.,.125,.5,.875,1.}) for(double b:{0.,.125,.5,.875,1.}) for(double c:{0.,.125,.5,.875,1.}) {
                    const auto current=vertices({a,b,c});const auto plan=planGrass(current,indices,planet,1000,eye,policy,&source);
                    EXPECT_EQ(plan.candidates,baseline.candidates);EXPECT_EQ(plan.density,baseline.density);
                    ASSERT_EQ(plan.patches.size(),1u);EXPECT_EQ(plan.morphBounds,baseline.morphBounds);
                    EXPECT_EQ(plan.patches[0].a,baseline.patches[0].a);EXPECT_EQ(plan.patches[0].expectedCandidates,baseline.patches[0].expectedCandidates);
                    glm::dvec3 p[3];for(int i=0;i<3;++i) {
                        p[i]={current[9*i],current[9*i+1],current[9*i+2]};
                        EXPECT_LE(glm::length(p[i]-glm::dvec3(plan.morphBounds[0])),plan.morphBounds[0].w);
                    }
                    const double area=.5*glm::length(glm::cross(p[1]-p[0],p[2]-p[0]))*1000000;
                    largest=std::max(largest,area);
                    EXPECT_LE(area*plan.density,plan.patches[0].expectedCandidates);
                    EXPECT_LE(plan.candidates,4096u);++phases;
                }
                if(!collapsed) {
                    const double fineArea=.5*glm::length(glm::cross(fine[1]-fine[0],fine[2]-fine[0]))*1000000;
                    const double parentArea=.5*glm::length(glm::cross(parent[1]-parent[0],parent[2]-parent[0]))*1000000;
                    EXPECT_GT(largest,2*std::max(fineArea,parentArea)); // Uniform endpoints alone are insufficient.
                }
            }
            GrassFalloff tiny;tiny.enabled=true;tiny.capacity=tiny.budget=1;tiny.protectedMeters=100;tiny.sigmaMeters=40;tiny.density=2;
            const auto capped=planGrass(submerged,indices,planet,1000,eye,tiny,&source);
            EXPECT_TRUE(capped.falloff.nearInfeasible);EXPECT_LE(capped.candidates,1u);
            auto bad=source;bad.metersPerRadius=999;
            EXPECT_THROW(planGrass(submerged,indices,planet,1000,eye,{},&bad),std::invalid_argument);
            bad=source;bad.corners[0]=source.samples();
            EXPECT_THROW(planGrass(submerged,indices,planet,1000,eye,{},&bad),std::invalid_argument);
            bad=source;bad.targets.pop_back();
            EXPECT_THROW(planGrass(submerged,indices,planet,1000,eye,{},&bad),std::invalid_argument);
        }
    }
    EXPECT_EQ(phases,1000u);
    Fixture ordinary;EXPECT_TRUE(ordinary.plan().morphBounds.empty());
}
