#include "FrameTestSupport.h"
#include "rendering/runtime/terrain/reload/interactive/PendingReload.h"
#include <functional>
#include <map>
#include <sstream>
namespace {
using Row=std::map<std::string,std::string>;
std::vector<Row> publicationRows(const std::string& path) {
    const auto split=[](const std::string& s) {
        std::vector<std::string> v;std::istringstream input(s);std::string f;
        while(std::getline(input,f,',')) v.push_back(f);
        if(!s.empty() && s.back()==',') v.emplace_back();return v;
    };
    std::ifstream input(path);std::string line;std::getline(input,line);const auto header=split(line);
    std::vector<Row> rows;
    while(std::getline(input,line)) {
        const auto values=split(line);EXPECT_EQ(values.size(),header.size());if(values.size()!=header.size()) continue;
        Row row;for(std::size_t i=0;i<header.size();++i) row[header[i]]=values[i];rows.push_back(row);
    }
    return rows;
}
std::uint64_t frame=0;
app::CommandLineOptions reloadOptions(const std::string& name) {
    frame=0;auto o=options(name);o.performanceTrace=o.configPath+".frames.csv";return o;
}
void drawReload(auto& r,bool character=true) {
    auto& p=r.profiler.publications();p.frame(++frame);p.collect();
    r.profiler.gpuWork().frame(frame);
    rendering::GpuWorkProfiler::Binding timing(&r.profiler.gpuWork());
    for(const auto& row:publicationRows(r.options.performanceTrace+".publications.csv"))
        if(row.at("kind")=="reload" && std::stoull(row.at("epoch"))>r.terrainSceneEpoch)
            EXPECT_NE(row.at("outcome"),"published");
    draw(r,character);r.profiler.collect();
    for(const auto& row:publicationRows(r.options.performanceTrace+".publications.csv")) {
        if(row.at("outcome")!="published" || row.at("end_frame")!=std::to_string(frame) || row.at("parent_attempt").empty()) continue;
        const auto i=std::stoull(row.at("body"));ASSERT_LT(i,r.meshReady.size());const auto g=r.publicationGeneration(i);
        EXPECT_EQ(row.at("epoch"),std::to_string(r.terrainSceneEpoch));
        EXPECT_EQ(row.at("request_serial"),std::to_string(r.installedTerrainSerial[i]));
        EXPECT_EQ(row.at("land_field"),std::to_string(g.landField));EXPECT_EQ(row.at("land_topology"),std::to_string(g.landTopology));
        EXPECT_EQ(row.at("water_field"),std::to_string(g.waterField));EXPECT_EQ(row.at("water_topology"),std::to_string(g.waterTopology));
        EXPECT_EQ(row.at("land_revision"),std::to_string(g.landRevision));EXPECT_EQ(row.at("water_revision"),std::to_string(g.waterRevision));
        for(int c=0;c<3;++c) EXPECT_DOUBLE_EQ(std::stod(row.at(std::string("grass_eye_")+"xyz"[c])),g.grassEye[c]);
    }
}
void verifyReloads(auto& r,const std::map<unsigned,std::string>& expected) {
    r.profiler.publications().collect();const auto rows=publicationRows(r.options.performanceTrace+".publications.csv");
    std::map<std::string,Row> roots;std::map<unsigned,std::string> outcomes;
    for(const auto& row:rows) if(row.at("kind")=="reload") {
        EXPECT_TRUE(roots.emplace(row.at("attempt"),row).second);
        EXPECT_TRUE(outcomes.emplace(std::stoul(row.at("epoch")),row.at("outcome")).second);
        EXPECT_TRUE(row.at("body").empty());EXPECT_TRUE(row.at("parent_attempt").empty());
        if(row.at("outcome")=="published") {
            EXPECT_FALSE(row.at("scene_exchange_ms").empty());
            EXPECT_GE(std::stod(row.at("publication_ms")),std::stod(row.at("scene_exchange_ms")));
        } else EXPECT_TRUE(row.at("publication_ms").empty());
    }
    EXPECT_EQ(outcomes,expected);
    for(const auto& [id,root]:roots) {
        unsigned children=0;
        for(const auto& row:rows) if(row.at("parent_attempt")==id) {
            ++children;EXPECT_EQ(row.at("epoch"),root.at("epoch"));EXPECT_EQ(row.at("kind"),"terrain");
            if(root.at("outcome")=="published") {
                EXPECT_EQ(row.at("outcome"),"published");EXPECT_EQ(row.at("end_frame"),root.at("end_frame"));
                EXPECT_FALSE(row.at("cpu_start_ms").empty());EXPECT_FALSE(row.at("cpu_end_ms").empty());
                EXPECT_FALSE(row.at("gpu_submit_ms").empty());EXPECT_FALSE(row.at("gpu_ready_ms").empty());
            } else EXPECT_TRUE(row.at("publication_ms").empty());
        }
        EXPECT_EQ(std::to_string(children),root.at("child_attempts"));
    }
    EXPECT_EQ(r.profiler.publications().stats().dropped,0u);
    EXPECT_GT(r.profiler.gpuWork().stats().submitted,0u);
    EXPECT_EQ(r.profiler.gpuWork().stats().dropped,0u);
}
json readJson(const std::string& path) {json j;std::ifstream(path)>>j;return j;}
void writeJson(const std::string& path,const json& j) {std::ofstream(path)<<j.dump(2);}
std::string bytes(const std::string& path) {std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void reloadTick(auto& r) {
    r.profiler.publications().frame(++frame);
    r.profiler.gpuWork().frame(frame);r.profiler.collect();
    rendering::GpuWorkProfiler::Binding timing(&r.profiler.gpuWork());
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
        {"renderer",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
        {"version",reinterpret_cast<const char*>(glGetString(GL_VERSION))},
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
    auto o=reloadOptions("moving");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    r.options.renderTestMode=false;GlAudit audit;DeleteAudit deletion;json trace=json::array();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return idle(r);}));drawReload(r);trace.push_back(snapshot(r));
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
    EXPECT_GT(r.astronautState()["walked_m"].get<double>(),3);drawReload(r);trace.push_back(snapshot(r));
    auto changedOnDisk=j;changedOnDisk["scenario_name"]="Not the requested snapshot";writeJson(o.configPath,changedOnDisk);
    delay=false;ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==2;}));
    EXPECT_EQ(r.scene.scenario.planets[0].name,"moon");EXPECT_EQ(r.scene.scenario.name,"Delayed Moon first");
    EXPECT_TRUE(r.retiredResidentScene);EXPECT_TRUE(glIsBuffer(oldBuffer));
    r.profiler.publications().collect();
    for(const auto& row:publicationRows(o.performanceTrace+".publications.csv"))
        if(row.at("epoch")=="2") EXPECT_NE(row.at("outcome"),"published");
    drawReload(r);trace.push_back(snapshot(r));
    delay=true;
    j["planets"].erase(0);j["scenario_name"]="Superseded waiting request";writeJson(o.configPath,j);renderer.reload();
    j["scenario_name"]="Latest single Earth";j["planets"][0]["surface_noise"][0]["seed"]=203;
    writeJson(o.configPath,j);renderer.reload();failPoll=true;reloadTick(r);
    EXPECT_TRUE(r.pendingResidentReload);EXPECT_FALSE(r.sceneReloadPreparing());EXPECT_TRUE(glIsBuffer(oldBuffer));
    for(int i=0;i<5;++i) reloadTick(r);
    EXPECT_EQ(r.terrainSceneEpoch,2u);drawReload(r);trace.push_back(snapshot(r));
    delay=false;ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==4;}));
    EXPECT_EQ(r.scene.scenario.name,"Latest single Earth");EXPECT_EQ(r.scene.scenario.planets.size(),1u);
    EXPECT_EQ(r.sceneReloadSuperseded,1u);EXPECT_TRUE(deletion.saw(oldBuffer));
    drawReload(r);trace.push_back(snapshot(r));verifyReloads(r,{{2,"published"},{3,"superseded"},{4,"published"}});saveReload(o,trace,audit,r);
}
TEST(TerrainReloadFrame, InvalidLatestRequestAndGpuFailureRetainLiveSceneThenIdenticalFieldSupersessionRecovers) {
    auto o=reloadOptions("failure");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    r.options.renderTestMode=false;GlAudit audit;json trace=json::array();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return idle(r);}));drawReload(r);trace.push_back(snapshot(r));
    const auto old=r.meshes.planetMeshes[0].vbo;const auto pose=r.astronautState();
    auto j=readJson(o.configPath);j["scenario_name"]="Discarded staged request";writeJson(o.configPath,j);
    delay=true;renderer.reload();ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.pendingResidentReload->gpuBody.has_value();}));
    j["planets"][0]["radius"]=-1;writeJson(o.configPath,j);
    EXPECT_THROW(renderer.reload(),std::invalid_argument);EXPECT_FALSE(r.pendingResidentReload);
    EXPECT_EQ(r.terrainSceneEpoch,1u);EXPECT_EQ(r.astronautState(),pose);EXPECT_TRUE(glIsBuffer(old));
    j["planets"][0]["radius"]=1;writeJson(o.configPath,j);delay=false;failFence=true;renderer.reload();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return !r.pendingResidentReload;}));
    EXPECT_EQ(r.sceneReloadFailures,2u);EXPECT_EQ(r.terrainSceneEpoch,1u);EXPECT_EQ(r.meshes.planetMeshes[0].vbo,old);
    drawReload(r);trace.push_back(snapshot(r));
    j["scenario_name"]="Same field old request";writeJson(o.configPath,j);delay=true;renderer.reload();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.pendingResidentReload->gpuBody.has_value();}));
    const auto obsoleteEpoch=r.pendingResidentReload->epoch;
    j["scenario_name"]="Same field latest request";writeJson(o.configPath,j);renderer.reload();
    EXPECT_GT(r.pendingResidentReload->epoch,obsoleteEpoch);delay=false;
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==6;}));
    EXPECT_EQ(r.scene.scenario.name,"Same field latest request");EXPECT_EQ(r.sceneReloadSuperseded,2u);
    drawReload(r);trace.push_back(snapshot(r));verifyReloads(r,{{2,"superseded"},{3,"invalid_config"},{4,"preparation_failed"},{5,"superseded"},{6,"published"}});saveReload(o,trace,audit,r);
}
TEST(TerrainReloadFrame, FinalExchangeFenceFailurePreservesPoseAndSavedReplayRecoversExactly) {
    auto o=reloadOptions("replay");rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
    const auto image=bytes(o.outputImagePath);auto& r=Probe::state(renderer);const auto saved=readJson(o.outputImagePath+".json");
    r.options.renderTestMode=false;r.options.replayPath=o.outputImagePath+".json";
    json trace=json::array();
    {
        GlAudit audit;drawReload(r);const auto pose=r.astronautState();const auto old=r.meshes.planetMeshes[0].vbo;
        bool reachedCommit=false;
        {
            CommitFault fault([&]{const auto* p=r.pendingResidentReload.get();
                if(p && p->planned && p->nextGrass==p->tracking.ready.size()) {reachedCommit=true;return true;}return false;});
            renderer.reload();ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return !r.pendingResidentReload;}));
        }
        EXPECT_TRUE(reachedCommit);EXPECT_EQ(r.sceneReloadFailures,1u);EXPECT_EQ(r.terrainSceneEpoch,1u);
        EXPECT_EQ(r.astronautState(),pose);EXPECT_TRUE(glIsBuffer(old));drawReload(r);trace.push_back(snapshot(r));
        auto invalid=saved;invalid["astronaut_pose"]["grass_trail"]={{{0,0},{0,0,0}}};
        writeJson(r.options.replayPath,invalid);EXPECT_THROW(renderer.reload(),std::invalid_argument);
        EXPECT_EQ(r.astronautState(),pose);writeJson(r.options.replayPath,saved);renderer.reload();
        ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==4;}));
        drawReload(r);EXPECT_EQ(r.astronautState()["root"],saved["astronaut_pose"]["root"]);
        EXPECT_EQ(r.astronautState()["effect_s"],saved["astronaut_pose"]["effect_s"]);
        trace.push_back(snapshot(r));verifyReloads(r,{{2,"preparation_failed"},{3,"invalid_config"},{4,"published"}});saveReload(o,trace,audit,r);
    }
    r.options.renderTestMode=true;ASSERT_EQ(renderer.run(),0);EXPECT_EQ(bytes(o.outputImagePath),image);
}
TEST(TerrainReloadFrame, RequestDuringStartupAndOptionalCameraEmptySceneReloadsRemainUsable) {
    auto o=reloadOptions("startup");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    r.options.renderTestMode=false;GlAudit audit;json trace=json::array();auto j=readJson(o.configPath);
    const auto original=j;j["planets"].erase(1);j["scenario_name"]="Reload requested during startup";
    writeJson(o.configPath,j);renderer.reload();reloadTick(r);
    EXPECT_TRUE(r.pendingResidentReload);EXPECT_FALSE(r.sceneReloadPreparing());EXPECT_FALSE(r.astronaut.motion.ready());
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==2;}));
    EXPECT_EQ(r.scene.scenario.planets.size(),1u);drawReload(r);trace.push_back(snapshot(r));
    j["planets"]=json::array();j.erase("surface_camera");j["scenario_name"]="Only Sun";writeJson(o.configPath,j);renderer.reload();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==3;}));
    EXPECT_FALSE(r.scene.surfaceCamera);EXPECT_EQ(r.cameraInput.mode(),CameraMode::Orbit);drawReload(r,false);trace.push_back(snapshot(r));
    writeJson(o.configPath,original);renderer.reload();
    ASSERT_TRUE(until([&]{reloadTick(r);},[&]{return r.terrainSceneEpoch==4;}));
    EXPECT_EQ(r.scene.scenario.planets.size(),2u);drawReload(r,false);trace.push_back(snapshot(r));verifyReloads(r,{{2,"published"},{3,"published"},{4,"published"}});saveReload(o,trace,audit,r);
}
