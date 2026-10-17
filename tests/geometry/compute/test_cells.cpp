#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "rendering/foliage/cells/CellIdentity.h"
#include "rendering/geometry/Mesh.h"
#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <map>
#include <optional>
#include <thread>
#include <bit>
#include <numeric>

using namespace rendering;
namespace {
using Key=std::array<std::uint32_t,3>;
Key key(const glm::uvec4& k) {return {k.x,k.y,k.z};}
struct Source:Mesh {
    Source(bool reverse=false,std::optional<Key> dirty={}) {
        hasVertexColors=true;
        for(int y=-7;y<8;++y) for(int x=-7;x<8;++x) for(int half=0;half<2;++half) {
            const glm::dvec3 a(x*.04,y*.04,1),b((x+1)*.04,y*.04,1),c((x+1)*.04,(y+1)*.04,1),d(x*.04,(y+1)*.04,1);
            std::array<glm::dvec3,3> p=half?std::array{a,c,d}:std::array{a,b,c};
            if(reverse) {std::rotate(p.begin(),p.begin()+1,p.end());std::swap(p[0],p[1]);}
            std::array<std::array<double,3>,3> radial;
            for(int j=0;j<3;++j) {const auto r=glm::normalize(p[j]);radial[j]={r.x,r.y,r.z};}
            const auto id=grassCellIdentity(radial);grassCellKeys.push_back(id);
            const bool moved=dirty && Key{id[0],id[1],id[2]}==*dirty;
            const auto first=unsigned(vertices.size()/9);
            for(auto v:p) {if(moved) v*=1.001;for(float f:{float(v.x),float(v.y),float(v.z),0.f,0.f,1.f,.3f,.8f,.3f}) vertices.push_back(f);}
            indices.insert(indices.end(),{first,first+1,first+2});
        }
        if(reverse) {
            std::vector<unsigned> reordered;std::reverse(grassCellKeys.begin(),grassCellKeys.end());
            for(std::size_t i=indices.size();i>0;i-=3) reordered.insert(reordered.end(),indices.begin()+i-3,indices.begin()+i);
            indices.swap(reordered);
        }
        upload();residentTerrain=true;
        terrainStats.generation={1,reverse?2ull:1ull,PlanetField::version,1,TerrainBackend::Compute};
        terrainStats.gpuCorners=indexCount;terrainStats.gpuCellKeys=indexCount/3;
    }
    ~Source() {destroy();}
};
config::PlanetConfig planet() {
    config::ScenarioConfig scene(config::Config::load("tests/scenarios/foliage/surface.json"));auto p=scene.planets[0];
    p.radius=.1;p.water.enabled=p.terrain_landscape.enabled=false;p.surface_noise.clear();p.color={1,1,1};
    auto& f=p.foliage;f.enabled=true;f.density_per_m2=2;f.draw_distance_m=20;f.quad_distance_m=8;
    f.max_blades=100000;f.max_candidates_per_triangle=65536;f.frustum_culling=false;return p;
}
void install(ProceduralGrass& grass,Source& source,const config::PlanetConfig& p,glm::dvec3 eye={0,0,1.02}) {
    auto stage=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,eye,source.revision);
    grass.waitForCapture(*stage);grass.reserve(0);grass.commit(0,*stage,source);
}
void prefixes(ProceduralGrass& grass,const std::vector<GrassCellRecord>& old,const std::vector<GrassRootRecord>& roots) {
    std::map<Key,GrassCellRecord> before;for(const auto& cell:old) ASSERT_TRUE(before.emplace(key(cell.key),cell).second);
    const auto after=grass.cachedCellsForValidation(0);const auto next=grass.cachedRootsForValidation(0);
    std::size_t compared=0;
    for(const auto& cell:after) {
        const auto found=before.find(key(cell.key));if(found==before.end() || cell.geometry!=found->second.geometry) continue;
        const auto& a=found->second;
        for(unsigned i=0;i<std::min(a.span.y,cell.span.y);++i) {
            const auto& x=roots[a.span.x+i];const auto& y=next[cell.span.x+i];
            EXPECT_EQ(glm::vec3(x.rootExpected),glm::vec3(y.rootExpected));EXPECT_EQ(x.upRank,y.upRank);
            EXPECT_EQ(x.normalSlot,y.normalSlot);EXPECT_EQ(x.variation,y.variation);++compared;
        }
    }
    EXPECT_GT(compared,0u);
}
}
TEST(GrassCells, MotionAndBudgetGrowthKeepOverlappingBladeIdentitiesAndRetireDepartingCells) {
    auto p=planet();Source source;ProceduralGrass grass;grass.enableAdaptiveBudget();install(grass,source,p);
    const auto old=grass.cachedCellsForValidation(0);const auto roots=grass.cachedRootsForValidation(0);
    ASSERT_FALSE(old.empty());install(grass,source,p,{.1,0,1.02});
    const auto moved=grass.stats(0);EXPECT_GT(moved.cellUpdates[0],0u);EXPECT_GT(moved.cellUpdates[1],0u);
    EXPECT_GT(moved.cellUpdates[3],0u);EXPECT_GT(moved.cellUpdates[4],0u);EXPECT_GT(moved.cellUpdates[5],0u);
    prefixes(grass,old,roots);
    auto cells=grass.cachedCellsForValidation(0);auto values=grass.cachedRootsForValidation(0);
    p.foliage.density_per_m2*=2;install(grass,source,p,{.1,0,1.02});
    EXPECT_GT(grass.stats(0).cellUpdates[5],0u);EXPECT_GT(grass.stats(0).cellUpdates[4],0u);prefixes(grass,cells,values);
    cells=grass.cachedCellsForValidation(0);values=grass.cachedRootsForValidation(0);
    p.foliage.density_per_m2*=.5;install(grass,source,p,{.1,0,1.02});
    EXPECT_EQ(grass.stats(0).cellUpdates[5],0u);prefixes(grass,cells,values);
    // Shared cell edges have one owner and unique identities; every valid root
    // remains on the synthetic surface even after selection/slot changes.
    std::map<Key,int> identities;for(const auto& cell:grass.cachedCellsForValidation(0)) EXPECT_EQ(++identities[key(cell.key)],1);
    for(const auto& root:grass.cachedRootsForValidation(0)) if(root.normalSlot.w!=0) EXPECT_FLOAT_EQ(root.rootExpected.z,1.f);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GrassCells, ReorderedTerrainRetainsCellsAndOnlyDirtyFootprintsRegenerate) {
    auto p=planet();Source original;ProceduralGrass grass;grass.enableAdaptiveBudget();install(grass,original,p);
    auto old=grass.cachedCellsForValidation(0);auto roots=grass.cachedRootsForValidation(0);
    Source reordered(true);install(grass,reordered,p);
    EXPECT_EQ(grass.stats(0).cellUpdates[5],0u);EXPECT_EQ(grass.stats(0).cellUpdates[2],0u);prefixes(grass,old,roots);
    old=grass.cachedCellsForValidation(0);roots=grass.cachedRootsForValidation(0);const auto target=key(old[0].key);
    Source dirty(true,target);install(grass,dirty,p);
    EXPECT_EQ(grass.stats(0).cellUpdates[2],1u);EXPECT_GT(grass.stats(0).cellUpdates[4],0u);EXPECT_GT(grass.stats(0).cellUpdates[5],0u);
    prefixes(grass,old,roots);const auto changed=grass.cachedCellsForValidation(0);const auto next=grass.cachedRootsForValidation(0);
    for(const auto& cell:changed) if(key(cell.key)==target)
        for(unsigned i=0;i<cell.span.y;++i) if(next[cell.span.x+i].normalSlot.w!=0) EXPECT_FLOAT_EQ(next[cell.span.x+i].rootExpected.z,1.001f);
    ++p.foliage.seed;install(grass,dirty,p);EXPECT_EQ(grass.stats(0).cellUpdates[4],0u);
    EXPECT_GT(grass.stats(0).cellUpdates[2],0u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GrassCells, PreparationSlicesRootWorkAndSourceLeaseSurvivesClearAndCancellation) {
    auto p=planet();p.foliage.density_per_m2=1024;p.foliage.max_blades=500000;
    Source source;ProceduralGrass grass;grass.enableAdaptiveBudget();install(grass,source,p);
    ASSERT_GT(grass.stats(0).candidates,4*65536u);
    auto stage=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,{.01,0,1.02},source.revision);
    const auto old=grass.stats(0).candidates;std::size_t calls=0,previous=0;
    for(;;) {
        // Count the completing poll too. Software drivers may signal its fence
        // immediately; slicing must not depend on an extra asynchronous poll.
        ASSERT_LT(++calls,1000000u);
        const bool ready=grass.poll(*stage);
        const auto dispatched=stage->stats().rootDispatches;EXPECT_LE(dispatched-previous,4u);previous=dispatched;
        if(ready) break;
        EXPECT_EQ(grass.stats(0).candidates,old);glFlush();std::this_thread::yield();
    }
    EXPECT_GT(calls,1u);EXPECT_GT(previous,4u);EXPECT_GT(stage->stats().cellUpdates[4],0u);
    stage.reset();EXPECT_EQ(grass.stats(0).candidates,old); // Cancel without changing live cells.
    stage=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,{.01,0,1.02},source.revision);
    source.residentTerrain=false;
    EXPECT_THROW(grass.prepare(0,source,p,1000,{.01,0,1.02}),std::logic_error);
    source.residentTerrain=true;
    grass.clear();grass.waitForCapture(*stage);EXPECT_TRUE(stage->ready());EXPECT_GT(stage->stats().cellUpdates[4],0u);
    stage.reset();EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GrassCells, SavedPolicyDefaultsToLegacyIdentityAndRejectsUnknownVersions) {
    auto p=planet();Source source;ProceduralGrass grass;grass.enableAdaptiveBudget();install(grass,source,p);
    nlohmann::json replay={{"render",{{"foliage_policy",nlohmann::json::array({grass.policy(0)})},
        {"foliage_cell_identity_version",1}}}};
    replay["render"]["foliage_policy"][0]["body"]=p.name;
    grass.restorePolicies(replay,{p});EXPECT_TRUE(grass.cellIdentity());
    replay["render"].erase("foliage_cell_identity_version");
    grass.restorePolicies(replay,{p});EXPECT_FALSE(grass.cellIdentity());
    for(int version:{-1,2}) {
        replay["render"]["foliage_cell_identity_version"]=version;
        EXPECT_THROW(grass.restorePolicies(replay,{p}),std::invalid_argument);
    }
    grass.restorePolicies(nlohmann::json::object(),{p});EXPECT_TRUE(grass.cellIdentity());
}

TEST(GrassCells, ImmutableSourceKeepsSlicedRootAndCellPreparationStableWhileLivePositionsChange) {
    auto p=planet();p.foliage.density_per_m2=1024;p.foliage.max_blades=500000;
    Source source;
    source.terrainStats.generation.topologyVersion=4;
    source.terrainStats.uniqueSamples=source.vertices.size()/9;
    source.morphSamples=source.terrainStats.uniqueSamples;
    std::vector<std::uint32_t> words(18*source.morphSamples,0),corners(source.indexCount);
    std::iota(corners.rbegin(),corners.rend(),0); // Deliberately nonidentity indirection.
    for(std::size_t i=0;i<source.morphSamples;++i) for(int j=0;j<3;++j)
        words[9*source.morphSamples+9*corners[i]+6+j]=std::bit_cast<std::uint32_t>(source.vertices[9*i+j]);
    glGenBuffers(1,&source.morphTargets);glBindBuffer(GL_ARRAY_BUFFER,source.morphTargets);
    glBufferData(GL_ARRAY_BUFFER,words.size()*4,words.data(),GL_STATIC_DRAW);
    glGenBuffers(1,&source.morphCorners);glBindBuffer(GL_ARRAY_BUFFER,source.morphCorners);
    glBufferData(GL_ARRAY_BUFFER,corners.size()*4,corners.data(),GL_STATIC_DRAW);
    struct TextureSentinels {
        GLuint ids[2]{};
        ~TextureSentinels(){glDeleteTextures(2,ids);glActiveTexture(GL_TEXTURE0);}
    } sentinels;
    glGenTextures(2,sentinels.ids);
    for(int i=0;i<2;++i) {
        glActiveTexture(GL_TEXTURE12+i);glBindTexture(GL_TEXTURE_BUFFER,sentinels.ids[i]);
        glTexBuffer(GL_TEXTURE_BUFFER,GL_R32UI,i==0?source.morphTargets:source.morphCorners);
    }
    glActiveTexture(GL_TEXTURE7);
    const auto assertTextures=[&] {
        GLint active=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);EXPECT_EQ(active,GL_TEXTURE7);
        for(int i=0;i<2;++i) {
            glActiveTexture(GL_TEXTURE12+i);GLint texture=0;glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&texture);
            EXPECT_EQ(texture,sentinels.ids[i]);
        }
        glActiveTexture(GL_TEXTURE7);
    };
    ProceduralGrass grass;grass.enableAdaptiveBudget();
    auto first=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,{0,0,1.02},source.revision,
        0,ProceduralGrass::defaultStageBytes,source.morphSource());
    grass.waitForCapture(*first);grass.reserve(0);grass.commit(0,*first,source);first.reset();
    ASSERT_GT(grass.stats(0).candidates,4*65536u);
    // A seed change prevents prefix copies from hiding incorrect position fetches.
    // Compare a frozen-source baseline with a second, fully fresh sliced build.
    ++p.foliage.seed;
    auto baseline=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,{0,0,1.02},source.revision,
        0,ProceduralGrass::defaultStageBytes,source.morphSource());
    grass.waitForCapture(*baseline);grass.commit(0,*baseline,source);baseline.reset();
    const auto expectedCells=grass.cachedCellsForValidation(0);const auto expectedRoots=grass.cachedRootsForValidation(0);
    ++p.foliage.seed;
    auto intervening=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,{0,0,1.02},source.revision,
        0,ProceduralGrass::defaultStageBytes,source.morphSource());
    grass.waitForCapture(*intervening);grass.commit(0,*intervening,source);intervening.reset();
    --p.foliage.seed;
    auto stage=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,{0,0,1.02},source.revision,
        0,ProceduralGrass::defaultStageBytes,source.morphSource());
    std::size_t calls=0,previous=0;auto live=source.vertices;
    for(;;) {
        ASSERT_LT(++calls,1000000u);
        for(std::size_t i=2;i<live.size();i+=9) live[i]=1.f+float((calls%7)+1)*.001f;
        glBindBuffer(GL_ARRAY_BUFFER,source.vbo);glBufferSubData(GL_ARRAY_BUFFER,0,live.size()*4,live.data());
        const bool ready=grass.poll(*stage);assertTextures();
        const auto dispatched=stage->stats().rootDispatches;EXPECT_LE(dispatched-previous,4u);previous=dispatched;
        if(ready) break;
        glFlush();std::this_thread::yield();
    }
    EXPECT_GT(calls,1u);EXPECT_EQ(stage->stats().cellUpdates[4],0u);
    grass.commit(0,*stage,source);stage.reset();
    const auto cells=grass.cachedCellsForValidation(0);const auto roots=grass.cachedRootsForValidation(0);
    ASSERT_EQ(cells.size(),expectedCells.size());ASSERT_EQ(roots.size(),expectedRoots.size());
    for(std::size_t i=0;i<cells.size();++i) {
        EXPECT_EQ(cells[i].key,expectedCells[i].key);EXPECT_EQ(cells[i].geometry,expectedCells[i].geometry);
        EXPECT_EQ(cells[i].metric,expectedCells[i].metric);
    }
    for(std::size_t i=0;i<roots.size();++i) {
        EXPECT_EQ(roots[i].rootExpected,expectedRoots[i].rootExpected);EXPECT_EQ(roots[i].upRank,expectedRoots[i].upRank);
        EXPECT_EQ(roots[i].normalSlot,expectedRoots[i].normalSlot);EXPECT_EQ(roots[i].variation,expectedRoots[i].variation);
        if(roots[i].normalSlot.w!=0) EXPECT_FLOAT_EQ(roots[i].rootExpected.z,1.f);
    }
    std::map<Key,unsigned> originalFaces;
    for(unsigned face=0;face<source.grassCellKeys.size();++face) {
        const auto& id=source.grassCellKeys[face];originalFaces.emplace(Key{id[0],id[1],id[2]},face);
    }
    for(const auto& cell:cells) {
        const auto found=originalFaces.find(key(cell.key));ASSERT_NE(found,originalFaces.end());
        glm::dvec3 triangle[3];
        for(int j=0;j<3;++j) {
            const auto offset=9*source.indices[3*found->second+j];
            triangle[j]={source.vertices[offset],source.vertices[offset+1],source.vertices[offset+2]};
        }
        for(unsigned slot=0;slot<cell.span.y;++slot) {
            const auto& root=roots[cell.span.x+slot];if(root.normalSlot.w==0) continue;
            for(int j=0;j<3;++j) EXPECT_GE(glm::cross(triangle[(j+1)%3]-triangle[j],
                glm::dvec3(root.rootExpected)-triangle[j]).z,-1e-8);
        }
    }
    // Re-enabling delta reuse must retain exact original cell geometry despite
    // the live VBO still containing different positions.
    auto delta=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,{0,0,1.02},source.revision,
        0,ProceduralGrass::defaultStageBytes,source.morphSource());
    grass.clear();grass.waitForCapture(*delta);EXPECT_GT(delta->stats().cellUpdates[4],0u);
    delta.reset(); // Source leases survive clear/cancellation.
    source.upload();EXPECT_FALSE(source.morphSource());
    grass.prepare(0,source,p,1000,{0,0,1.02});
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    std::cout << "IMMUTABLE_GRASS_ROOTS roots=" << roots.size() << " cells=" << cells.size()
        << " sliced_polls=" << calls << " live_height_updates=" << calls << '\n';
}
