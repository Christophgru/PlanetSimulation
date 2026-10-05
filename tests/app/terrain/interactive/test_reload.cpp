#include "FrameTestSupport.h"
#include "rendering/runtime/terrain/reload/interactive/PendingReload.h"
#include <functional>
namespace {
json readJson(const std::string& path) {json j;std::ifstream(path)>>j;return j;}
void writeJson(const std::string& path,const json& j) {std::ofstream(path)<<j.dump(2);}
std::string bytes(const std::string& path) {std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void reloadTick(auto& r) {
    // Inspect the exchange before a subsequent frame can retire the old scene.
    if(r.pollResidentReload()) return;
    const auto e=r.scene.surfaceCamera ? r.scene.surfaceCamera->position() : glm::dvec3(r.scene.sunCamera.position);
    r.preparePlanetMeshes(e,true,r.scene.surfaceCamera ? std::optional<double>(0) : std::nullopt);
}
json snapshot(auto& r) {
    return {{"reload",r.sceneReloadState()},{"publication",r.terrainPublicationState()},
        {"body_count",r.scene.scenario.planets.size()},{"name",r.scene.scenario.name}};
}
void saveReload(const app::CommandLineOptions& o,const json& trace,const GlAudit& audit,auto& r) {
    const auto w=r.terrainJobs.stats();EXPECT_LE(w.running,1u);EXPECT_LE(w.queued,1u);EXPECT_LE(w.ready,1u);
    EXPECT_EQ(w.peakRunning,1u);EXPECT_EQ(w.peakQueued,1u);
    std::ofstream(o.configPath+".results.json")<<json{{"frames",trace},{"gl",audit.state()},
        {"worker",{{"peak_running",w.peakRunning},{"peak_queued",w.peakQueued},{"obsolete",w.obsolete}}}}.dump(2);
}
PFNGLDELETEBUFFERSPROC originalDelete;
std::vector<GLuint> deleted;
void GLAPIENTRY trackDelete(GLsizei n,const GLuint* ids) {
    deleted.insert(deleted.end(),ids,ids+n);originalDelete(n,ids);
}
struct DeleteAudit {
    DeleteAudit(){deleted.clear();originalDelete=__glewDeleteBuffers;__glewDeleteBuffers=trackDelete;}
    ~DeleteAudit(){__glewDeleteBuffers=originalDelete;}
    bool saw(GLuint id) const {return std::find(deleted.begin(),deleted.end(),id)!=deleted.end();}
};
std::function<bool()> commitFailure;
PFNGLFENCESYNCPROC commitFence;
GLsync GLAPIENTRY failCommit(GLenum condition,GLbitfield flags) {
    if(commitFailure && commitFailure()) {commitFailure={};return nullptr;}
    return commitFence(condition,flags);
}
struct CommitFault {
    explicit CommitFault(std::function<bool()> predicate) {
        commitFailure=std::move(predicate);commitFence=__glewFenceSync;__glewFenceSync=failCommit;
    }
    ~CommitFault(){__glewFenceSync=commitFence;commitFailure={};}
};
}
TEST(TerrainReloadFrame, MovingOldSceneDrawsDuringDelayedPreparationAndLatestRequestSurvivesRetirement) {
    auto o=options("moving");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    r.options.renderTestMode=false;GlAudit audit;DeleteAudit deletion;json trace=json::array();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return idle(r);}));draw(r);trace.push_back(snapshot(r));
    const auto oldBuffer=r.meshes.planetMeshes[0].vbo;
    auto j=readJson(o.configPath);std::swap(j["planets"][0],j["planets"][1]);j["scenario_name"]="Delayed Moon first";
    writeJson(o.configPath,j);delay=true;renderer.reload();
    EXPECT_EQ(r.terrainSceneEpoch,1u);EXPECT_TRUE(r.pendingResidentReload);
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.pendingResidentReload->gpuBody.has_value();}));
    for(int i=0;i<8;++i) {
        r.scene.surfaceCamera->walk(1,0,.5/(r.scene.surfaceCamera->walkSpeed()*r.scene.scenario.metersPerWorldUnit()));
        r.characterWindTime+=1.0/12;reloadTick(r);r.prepareAstronaut(1.0/12);
        EXPECT_EQ(r.terrainSceneEpoch,1u);EXPECT_EQ(r.meshes.planetMeshes[0].vbo,oldBuffer);
    }
    EXPECT_GT(r.astronautState()["walked_m"].get<double>(),3);draw(r);trace.push_back(snapshot(r));
    auto changedOnDisk=j;changedOnDisk["scenario_name"]="Not the requested snapshot";writeJson(o.configPath,changedOnDisk);
    delay=false;ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==2;}));
    EXPECT_EQ(r.scene.scenario.planets[0].name,"moon");EXPECT_EQ(r.scene.scenario.name,"Delayed Moon first");
    EXPECT_TRUE(r.retiredResidentScene);EXPECT_TRUE(glIsBuffer(oldBuffer));draw(r);trace.push_back(snapshot(r));
    delay=true;
    j["planets"].erase(0);j["scenario_name"]="Superseded waiting request";writeJson(o.configPath,j);renderer.reload();
    j["scenario_name"]="Latest single Earth";j["planets"][0]["surface_noise"][0]["seed"]=203;
    writeJson(o.configPath,j);renderer.reload();failPoll=true;reloadTick(r);
    EXPECT_TRUE(r.pendingResidentReload);EXPECT_FALSE(r.sceneReloadPreparing());EXPECT_TRUE(glIsBuffer(oldBuffer));
    for(int i=0;i<5;++i) reloadTick(r);
    EXPECT_EQ(r.terrainSceneEpoch,2u);draw(r);trace.push_back(snapshot(r));
    delay=false;ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==4;}));
    EXPECT_EQ(r.scene.scenario.name,"Latest single Earth");EXPECT_EQ(r.scene.scenario.planets.size(),1u);
    EXPECT_EQ(r.sceneReloadSuperseded,1u);EXPECT_TRUE(deletion.saw(oldBuffer));
    draw(r);trace.push_back(snapshot(r));saveReload(o,trace,audit,r);
}
TEST(TerrainReloadFrame, InvalidLatestRequestAndGpuFailureRetainLiveSceneThenIdenticalFieldSupersessionRecovers) {
    auto o=options("failure");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    r.options.renderTestMode=false;GlAudit audit;json trace=json::array();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return idle(r);}));draw(r);trace.push_back(snapshot(r));
    const auto old=r.meshes.planetMeshes[0].vbo;const auto pose=r.astronautState();
    auto j=readJson(o.configPath);j["scenario_name"]="Discarded staged request";writeJson(o.configPath,j);
    delay=true;renderer.reload();ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.pendingResidentReload->gpuBody.has_value();}));
    j["planets"][0]["radius"]=-1;writeJson(o.configPath,j);
    EXPECT_THROW(renderer.reload(),std::invalid_argument);EXPECT_FALSE(r.pendingResidentReload);
    EXPECT_EQ(r.terrainSceneEpoch,1u);EXPECT_EQ(r.astronautState(),pose);EXPECT_TRUE(glIsBuffer(old));
    j["planets"][0]["radius"]=1;writeJson(o.configPath,j);delay=false;failFence=true;renderer.reload();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return !r.pendingResidentReload;}));
    EXPECT_EQ(r.sceneReloadFailures,2u);EXPECT_EQ(r.terrainSceneEpoch,1u);EXPECT_EQ(r.meshes.planetMeshes[0].vbo,old);
    draw(r);trace.push_back(snapshot(r));
    j["scenario_name"]="Same field old request";writeJson(o.configPath,j);delay=true;renderer.reload();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.pendingResidentReload->gpuBody.has_value();}));
    const auto obsoleteEpoch=r.pendingResidentReload->epoch;
    j["scenario_name"]="Same field latest request";writeJson(o.configPath,j);renderer.reload();
    EXPECT_GT(r.pendingResidentReload->epoch,obsoleteEpoch);delay=false;
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==6;}));
    EXPECT_EQ(r.scene.scenario.name,"Same field latest request");EXPECT_EQ(r.sceneReloadSuperseded,2u);
    draw(r);trace.push_back(snapshot(r));saveReload(o,trace,audit,r);
}
TEST(TerrainReloadFrame, FinalExchangeFenceFailurePreservesPoseAndSavedReplayRecoversExactly) {
    auto o=options("replay");rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
    const auto image=bytes(o.outputImagePath);auto& r=Probe::state(renderer);const auto saved=readJson(o.outputImagePath+".json");
    r.options.renderTestMode=false;r.options.replayPath=o.outputImagePath+".json";
    json trace=json::array();
    {
        GlAudit audit;draw(r);const auto pose=r.astronautState();const auto old=r.meshes.planetMeshes[0].vbo;
        bool reachedCommit=false;
        {
            CommitFault fault([&]{const auto* p=r.pendingResidentReload.get();
                if(p && p->planned && p->nextGrass==p->tracking.ready.size()) {reachedCommit=true;return true;}return false;});
            renderer.reload();ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return !r.pendingResidentReload;}));
        }
        EXPECT_TRUE(reachedCommit);EXPECT_EQ(r.sceneReloadFailures,1u);EXPECT_EQ(r.terrainSceneEpoch,1u);
        EXPECT_EQ(r.astronautState(),pose);EXPECT_TRUE(glIsBuffer(old));draw(r);trace.push_back(snapshot(r));
        auto invalid=saved;invalid["astronaut_pose"]["grass_trail"]={{{0,0},{0,0,0}}};
        writeJson(r.options.replayPath,invalid);EXPECT_THROW(renderer.reload(),std::invalid_argument);
        EXPECT_EQ(r.astronautState(),pose);writeJson(r.options.replayPath,saved);renderer.reload();
        ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==4;}));
        draw(r);EXPECT_EQ(r.astronautState()["root"],saved["astronaut_pose"]["root"]);
        EXPECT_EQ(r.astronautState()["effect_s"],saved["astronaut_pose"]["effect_s"]);
        trace.push_back(snapshot(r));saveReload(o,trace,audit,r);
    }
    r.options.renderTestMode=true;ASSERT_EQ(renderer.run(),0);EXPECT_EQ(bytes(o.outputImagePath),image);
}
TEST(TerrainReloadFrame, RequestDuringStartupAndOptionalCameraEmptySceneReloadsRemainUsable) {
    auto o=options("startup");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    r.options.renderTestMode=false;GlAudit audit;json trace=json::array();auto j=readJson(o.configPath);
    const auto original=j;j["planets"].erase(1);j["scenario_name"]="Reload requested during startup";
    writeJson(o.configPath,j);renderer.reload();reloadTick(r);
    EXPECT_TRUE(r.pendingResidentReload);EXPECT_FALSE(r.sceneReloadPreparing());EXPECT_FALSE(r.astronaut.motion.ready());
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==2;}));
    EXPECT_EQ(r.scene.scenario.planets.size(),1u);draw(r);trace.push_back(snapshot(r));
    j["planets"]=json::array();j.erase("surface_camera");j["scenario_name"]="Only Sun";writeJson(o.configPath,j);renderer.reload();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==3;}));
    EXPECT_FALSE(r.scene.surfaceCamera);EXPECT_EQ(r.cameraInput.mode(),CameraMode::Orbit);draw(r,false);trace.push_back(snapshot(r));
    writeJson(o.configPath,original);renderer.reload();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==4;}));
    EXPECT_EQ(r.scene.scenario.planets.size(),2u);draw(r,false);trace.push_back(snapshot(r));saveReload(o,trace,audit,r);
}
