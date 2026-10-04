#include "app/Window.h"
#include "rendering/runtime/terrain/reload/SceneTerrainReplacement.h"
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace {
using namespace rendering;
using nlohmann::json;
json document() {
    json j;std::ifstream(std::string(PLANET_SOURCE_DIR)+"/tests/scenarios/foliage/surface.json")>>j;
    for(auto& p:j["planets"]) {
        p["terrain_lod"]["max_triangle_budget"]=10000;
        p["terrain_lod"]["shoreline_edge_m"]=0;
        if(p.contains("foliage")) p["foliage"]["max_blades"]=32;
    }
    return j;
}
struct LiveScene {
    json document;
    app::PreparedScene scene;
    std::vector<Mesh> land,water;
    ProceduralGrass grass;
    TerrainPublication publication;
    std::uint64_t epoch=1;
    explicit LiveScene(json j):document(std::move(j)),scene(config::ScenarioConfig{config::Config{json(document)}}),
        land(scene.scenario.planets.size()),water(land.size()),publication(grass,land.size()) {}
    ~LiveScene(){grass.clear();for(auto& m:land)m.destroy();for(auto& m:water)m.destroy();}
    SceneTerrainDestination destination(){return {scene,document,land,water,grass,publication,epoch};}
};
glm::dvec3 eye(const SceneTerrainReplacement& p) {return p.scene().surfaceCamera->position();}
void prepare(SceneTerrainReplacement& p,TerrainCompute& compute) {
    for(std::size_t i=0;i<p.scene().scenario.planets.size();++i) {
        auto r=p.request(i,eye(p),i+1);
        ASSERT_TRUE(p.submit(buildTerrainCpu(r),r.identity,r.identity.eye/r.planet.radius,compute));
        p.waitForCapture();
    }
    ASSERT_TRUE(p.ready());
}
void install(LiveScene& live,TerrainCompute& compute,json j) {
    const auto epoch=live.epoch+1;
    SceneTerrainReplacement p(std::move(j),live.destination(),epoch,20);
    prepare(p,compute);ASSERT_TRUE(p.publish(epoch));p.waitRetiredForCapture();
}
struct Snapshot {
    json document;
    std::uint64_t epoch,bytes;
    std::vector<GLuint> land,water;
    std::vector<TerrainGenerationKey> keys;
    explicit Snapshot(const LiveScene& live):document(live.document),epoch(live.epoch),bytes(live.publication.reservedBytes()) {
        for(std::size_t i=0;i<live.land.size();++i) {
            land.push_back(live.land[i].vbo);water.push_back(live.water[i].vbo);
            keys.push_back(live.land[i].terrainStats.generation);
        }
    }
    void unchanged(const LiveScene& live) const {
        EXPECT_EQ(live.document,document);EXPECT_EQ(live.epoch,epoch);ASSERT_EQ(live.land.size(),land.size());
        for(std::size_t i=0;i<land.size();++i) {
            EXPECT_EQ(live.land[i].vbo,land[i]);EXPECT_EQ(live.water[i].vbo,water[i]);
            EXPECT_EQ(live.land[i].terrainStats.generation,keys[i]);
            EXPECT_EQ(live.grass.residentGeneration(i),keys[i]);
            if(land[i]) EXPECT_TRUE(glIsBuffer(land[i]));
            if(water[i]) EXPECT_TRUE(glIsBuffer(water[i]));
        }
    }
};
class SceneReplacement : public testing::Test {
protected:
    app::CommandLineOptions options;
    std::unique_ptr<app::Window> window;
    void SetUp() override {
        options.renderTestMode=true;options.renderTestWidth=320;options.renderTestHeight=180;
        window=std::make_unique<app::Window>(options);
    }
    void TearDown() override {EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));window.reset();EXPECT_EQ(glfwGetCurrentContext(),nullptr);}
};
unsigned fenceCalls=0,failAt=0,pollCalls=0,blockingCalls=0;
PFNGLFENCESYNCPROC originalFence=nullptr;
GLsync GLAPIENTRY failFence(GLenum condition,GLbitfield flags) {
    if(++fenceCalls==failAt) return nullptr;
    return originalFence(condition,flags);
}
struct FenceFault {
    explicit FenceFault(unsigned call){fenceCalls=0;failAt=call;originalFence=__glewFenceSync;__glewFenceSync=failFence;}
    ~FenceFault(){__glewFenceSync=originalFence;}
};
GLenum GLAPIENTRY delayFence(GLsync,GLbitfield flags,GLuint64 timeout) {
    ++pollCalls;if(flags || timeout) ++blockingCalls;return GL_TIMEOUT_EXPIRED;
}
struct FenceDelay {
    PFNGLCLIENTWAITSYNCPROC original=__glewClientWaitSync;
    FenceDelay(){pollCalls=blockingCalls=0;__glewClientWaitSync=delayFence;}
    ~FenceDelay(){__glewClientWaitSync=original;}
};
PFNGLGENBUFFERSPROC originalGenBuffers=nullptr;
std::vector<GLuint> generated;
void GLAPIENTRY recordBuffers(GLsizei n,GLuint* buffers) {
    originalGenBuffers(n,buffers);generated.insert(generated.end(),buffers,buffers+n);
}
struct BufferProbe {
    BufferProbe(){generated.clear();originalGenBuffers=__glewGenBuffers;__glewGenBuffers=recordBuffers;}
    ~BufferProbe(){__glewGenBuffers=originalGenBuffers;}
};
void drawOld(const LiveScene& live) {
    glEnable(GL_RASTERIZER_DISCARD);
    live.grass.shader.use();
    live.grass.shader.setInt("uTerrainVertices",8);live.grass.shader.setInt("uTerrainIndices",9);
    live.grass.shader.setInt("uShadowMap",1);live.grass.shader.setInt("uAtmColumns",7);live.grass.shader.setInt("uTrailTree",10);
    for(std::size_t i=0;i<live.land.size();++i) {
        live.land[i].draw();live.water[i].draw();
        GrassPass pass;pass.mainEyeBody=*live.grass.planningEye(i);live.grass.draw(i,&pass);
    }
    glDisable(GL_RASTERIZER_DISCARD);glUseProgram(0);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
}
TEST_F(SceneReplacement, DelayedBodiesKeepOldSceneDrawableUntilCompleteExchangeAndFencedRetirement) {
    auto j=document();LiveScene live(j);TerrainCompute compute;install(live,compute,j);Snapshot old(live);
    live.grass.trail(0).restore({{{1000,0,0},{1000,.4,0}}});
    j["scenario_name"]="Replacement Moon";std::swap(j["planets"][0],j["planets"][1]);
    j["surface_camera"]["planet_index"]=0;j["planets"][1]["water"]["enabled"]=false;
    j["planets"][1]["foliage"]["enabled"]=false;
    SceneTerrainReplacement next(j,live.destination(),live.epoch+1,20);
    auto r=next.request(0,eye(next),1);
    ASSERT_TRUE(next.submit(buildTerrainCpu(r),r.identity,r.identity.eye/r.planet.radius,compute));
    {FenceDelay delay;EXPECT_FALSE(next.poll());EXPECT_GT(pollCalls,0u);EXPECT_EQ(blockingCalls,0u);old.unchanged(live);drawOld(live);}
    next.waitForCapture();EXPECT_FALSE(next.ready());old.unchanged(live);
    EXPECT_THROW(next.publish(live.epoch+1),std::logic_error);
    auto second=next.request(1,eye(next),2);
    ASSERT_TRUE(next.submit(buildTerrainCpu(second),second.identity,second.identity.eye/second.planet.radius,compute));
    next.waitForCapture();ASSERT_TRUE(next.ready());old.unchanged(live);drawOld(live);
    ASSERT_TRUE(next.publish(old.epoch+1));EXPECT_EQ(live.document,j);EXPECT_EQ(live.scene.scenario.planets[0].name,"moon");
    EXPECT_EQ(live.grass.existingTrail(0),nullptr);
    ASSERT_NE(next.grass().existingTrail(0),nullptr);
    EXPECT_EQ(next.grass().existingTrail(0)->segments().size(),1u);
    EXPECT_EQ(live.scene.scenario.surface_camera.planet_index,0);EXPECT_EQ(live.water[1].vbo,0u);
    EXPECT_GT(live.publication.externalBytes(),0u);
    {FenceDelay delay;EXPECT_FALSE(next.pollRetired());EXPECT_EQ(blockingCalls,0u);
        for(auto buffer:old.land) EXPECT_TRUE(glIsBuffer(buffer));
        EXPECT_THROW(SceneTerrainReplacement(j,live.destination(),live.epoch+1,20),std::invalid_argument);
        EXPECT_FALSE(live.publication.canSubmit(0));}
    next.waitRetiredForCapture();EXPECT_EQ(live.publication.externalBytes(),0u);
    EXPECT_EQ(next.grass().existingTrail(0),nullptr);
    for(auto buffer:old.land) EXPECT_FALSE(glIsBuffer(buffer));
    for(std::size_t i=0;i<live.land.size();++i) {
        EXPECT_EQ(live.publication.installed(i).identity.epoch,live.epoch);
        EXPECT_EQ(live.publication.installed(i).identity.bodyName,live.scene.scenario.planets[i].name);
        EXPECT_EQ(live.grass.residentGeneration(i),live.land[i].terrainStats.generation);
    }
}
TEST_F(SceneReplacement, SixBodyFenceFailuresDiscardAllStagingAndPermitANewReplacement) {
    auto j=document();LiveScene live(j);TerrainCompute compute;install(live,compute,j);Snapshot old(live);
    for(unsigned failure=1;failure<=6;++failure) {
        {BufferProbe buffers;SceneTerrainReplacement next(j,live.destination(),live.epoch+1,20);
            auto r=next.request(0,eye(next),1);auto built=buildTerrainCpu(r);FenceFault fault(failure);
            EXPECT_THROW({next.submit(std::move(built),r.identity,r.identity.eye/r.planet.radius,compute);next.waitForCapture();},std::runtime_error);
            old.unchanged(live);EXPECT_FALSE(generated.empty());}
        for(auto buffer:generated) EXPECT_FALSE(glIsBuffer(buffer));
        EXPECT_TRUE(live.publication.canSubmit(0));
    }
    install(live,compute,j);EXPECT_GT(live.epoch,old.epoch);
}
TEST_F(SceneReplacement, WholeSceneRetirementFenceFailureIsReversibleAndTheSameTransactionRecovers) {
    auto j=document();LiveScene live(j);TerrainCompute compute;install(live,compute,j);Snapshot old(live);
    SceneTerrainReplacement next(j,live.destination(),live.epoch+1,20);prepare(next,compute);
    {FenceFault fault(1);EXPECT_THROW(next.publish(old.epoch+1),std::runtime_error);}
    old.unchanged(live);ASSERT_TRUE(next.ready());drawOld(live);
    ASSERT_TRUE(next.publish(old.epoch+1));next.waitRetiredForCapture();
}
TEST_F(SceneReplacement, RetirementPollFailureKeepsOldResourcesChargedUntilSuccessfulRetry) {
    auto j=document();LiveScene live(j);TerrainCompute compute;install(live,compute,j);Snapshot old(live);
    SceneTerrainReplacement next(j,live.destination(),live.epoch+1,20);prepare(next,compute);
    ASSERT_TRUE(next.publish(old.epoch+1));const auto bytes=live.publication.reservedBytes();
    const auto original=__glewClientWaitSync;
    __glewClientWaitSync=+[](GLsync,GLbitfield,GLuint64)->GLenum {return GL_WAIT_FAILED;};
    EXPECT_THROW(next.pollRetired(),std::runtime_error);__glewClientWaitSync=original;
    EXPECT_EQ(live.publication.reservedBytes(),bytes);EXPECT_GT(live.publication.externalBytes(),0u);
    for(auto buffer:old.land) EXPECT_TRUE(glIsBuffer(buffer));
    next.waitRetiredForCapture();EXPECT_EQ(live.publication.externalBytes(),0u);
    for(auto buffer:old.land) EXPECT_FALSE(glIsBuffer(buffer));
}
TEST_F(SceneReplacement, SupersededEpochsAndChangedBodyIdentitiesNeverExchangeTheScene) {
    auto j=document();LiveScene live(j);TerrainCompute compute;install(live,compute,j);Snapshot old(live);
    {SceneTerrainReplacement next(j,live.destination(),live.epoch+1,20);
        auto r=next.request(0,eye(next),1);const auto built=buildTerrainCpu(r);BufferProbe buffers;
        for(int variant=0;variant<11;++variant) {
            auto k=r.identity;
            switch(variant) {
                case 0:++k.epoch;break;case 1:++k.serial;break;case 2:++k.bodyIndex;break;
                case 3:k.bodyName="other";break;case 4:++k.field;break;case 5:++k.fieldVersion;break;
                case 6:++k.topologyVersion;break;case 7:k.backend=TerrainBackend::Cpu;break;
                case 8:k.resident=false;break;case 9:k.eye.x+=.01;break;case 10:k.localMask^=1;break;
            }
            EXPECT_THROW(next.submit(built,k,k.eye/r.planet.radius,compute),std::invalid_argument);
        }
        EXPECT_TRUE(generated.empty());old.unchanged(live);
        EXPECT_FALSE(next.publish(old.epoch+2));
    }
    {SceneTerrainReplacement next(j,live.destination(),live.epoch+1,20);prepare(next,compute);
        EXPECT_FALSE(next.publish(old.epoch+2));old.unchanged(live);
        ++live.land[0].revision;EXPECT_THROW(next.publish(old.epoch+1),std::invalid_argument);--live.land[0].revision;
        auto contacts=live.land[0].contacts;live.land[0].contacts.reset();
        EXPECT_THROW(next.publish(old.epoch+1),std::invalid_argument);live.land[0].contacts=std::move(contacts);
        live.document["scenario_name"]="edited";EXPECT_THROW(next.publish(old.epoch+1),std::invalid_argument);
        live.document=old.document;
        old.unchanged(live);}
    EXPECT_TRUE(live.publication.canSubmit(0));
}
TEST_F(SceneReplacement, InvalidConfigsOverlapAndAdmissionFailureKeepOldResourcesAndDispatchNothing) {
    auto j=document();LiveScene live(j);TerrainCompute compute;install(live,compute,j);Snapshot old(live);
    auto invalid=j;invalid["planets"][0]["radius"]=-1;
    EXPECT_THROW(SceneTerrainReplacement(invalid,live.destination(),live.epoch+1,20),std::invalid_argument);
    EXPECT_THROW(SceneTerrainReplacement(j,live.destination(),live.epoch,20),std::invalid_argument);
    EXPECT_THROW(SceneTerrainReplacement(j,live.destination(),live.epoch+1,20,old.bytes-1),std::runtime_error);
    {SceneTerrainReplacement next(j,live.destination(),live.epoch+1,20,old.bytes);
        EXPECT_THROW(SceneTerrainReplacement(j,live.destination(),live.epoch+1,20),std::invalid_argument);
        auto r=next.request(0,eye(next),1);auto built=buildTerrainCpu(r);BufferProbe buffers;
        EXPECT_THROW(next.submit(std::move(built),r.identity,r.identity.eye/r.planet.radius,compute),std::runtime_error);
        EXPECT_TRUE(generated.empty());old.unchanged(live);}
    EXPECT_TRUE(live.publication.canSubmit(0));install(live,compute,j);
}
TEST_F(SceneReplacement, RepeatedReplacementChangesBodyCountOrderAndContactsWithoutAccumulatingRetiredScenes) {
    auto j=document();LiveScene live(j);TerrainCompute compute;install(live,compute,j);
    for(int cycle=0;cycle<6;++cycle) {
        auto next=j;next["scenario_name"]="cycle "+std::to_string(cycle);
        if(cycle%2==0) next["planets"].erase(1);
        else {std::swap(next["planets"][0],next["planets"][1]);next["surface_camera"]["planet_index"]=0;}
        next["planets"][cycle%2==0 ? 0 : 1]["surface_noise"][0]["seed"]=100+cycle;
        Snapshot old(live);install(live,compute,next);
        EXPECT_EQ(live.land.size(),cycle%2==0 ? 1u : 2u);EXPECT_EQ(live.document,next);
        EXPECT_EQ(live.publication.externalBytes(),0u);
        EXPECT_LT(live.publication.reservedBytes(),2*ProceduralGrass::defaultStageBytes);
        for(auto buffer:old.land) EXPECT_FALSE(glIsBuffer(buffer));
        live.scene.surfaceCamera->walk(1,0,.001);drawOld(live);
    }
}
