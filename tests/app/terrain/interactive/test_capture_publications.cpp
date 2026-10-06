#include "FrameTestSupport.h"
#include <map>
#include <set>
#include <sstream>
namespace {
using Row=std::map<std::string,std::string>;
std::vector<Row> captureRows(const std::string& path) {
    const auto split=[](const std::string& s) {
        std::istringstream in(s);std::string f;std::vector<std::string> v;
        while(std::getline(in,f,',')) v.push_back(f);
        if(!s.empty() && s.back()==',') v.emplace_back();return v;
    };
    std::ifstream in(path);std::string line;std::getline(in,line);const auto keys=split(line);std::vector<Row> rows;
    while(std::getline(in,line)) {
        const auto values=split(line);EXPECT_EQ(values.size(),keys.size());if(values.size()!=keys.size()) continue;
        Row r;for(std::size_t i=0;i<keys.size();++i) r[keys[i]]=values[i];rows.push_back(r);
    }
    return rows;
}
void captureCase(const std::string& mode) {
    auto o=options("capture-publication-"+mode);o.performanceTrace=o.configPath+".frames.csv";
    o.thirdPersonRenderMode=false;o.benchmarkFrames=2;o.benchmarkStep=0;
    if(mode!="resident") {o.terrainBackend=mode=="cpu" ? "cpu" : "compute";o.terrainGrassPlanner="cpu";}
    {
        rendering::Renderer renderer(o);auto& r=Probe::state(renderer);ASSERT_EQ(renderer.run(),0);
        auto& p=r.profiler.publications();p.collect();auto rows=captureRows(o.performanceTrace+".publications.csv");
        unsigned startup=0;for(const auto& row:rows) if(row.at("kind")=="terrain") {
            ++startup;EXPECT_EQ(row.at("outcome"),"published");EXPECT_EQ(row.at("capture_mode"),"1");
            EXPECT_EQ(row.at("end_frame"),"0");const auto i=std::stoull(row.at("body"));ASSERT_LT(i,r.meshReady.size());
            EXPECT_EQ(row.at("land_revision"),std::to_string(r.meshes.planetMeshes[i].revision));
            EXPECT_EQ(row.at("request_serial"),std::to_string(r.installedTerrainSerial[i]));
        }
        EXPECT_EQ(startup,2u);const auto begun=p.stats().begun;
        ASSERT_EQ(renderer.run(),0);EXPECT_EQ(p.stats().begun,begun); // Cached presentations add no admissions.
        json j;std::ifstream(o.configPath)>>j;const auto good=j;j["planets"][0]["radius"]=-1;
        std::ofstream(o.configPath)<<j.dump(2);EXPECT_THROW(renderer.reload(),std::invalid_argument);
        std::ofstream(o.configPath)<<good.dump(2);renderer.reload();EXPECT_EQ(r.terrainSceneEpoch,2u);
        p.collect();rows=captureRows(o.performanceTrace+".publications.csv");
        for(const auto& row:rows) if(row.at("kind")=="reload") EXPECT_NE(row.at("outcome"),"published");
        // A complete exchange replaced before any draw must remain unsuccessful.
        renderer.reload();EXPECT_EQ(r.terrainSceneEpoch,3u);ASSERT_EQ(renderer.run(),0);p.collect();
        EXPECT_EQ(p.stats().dropped,0u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
        std::ofstream(o.configPath+".receipts.json")<<json{{"renderer",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
            {"version",reinterpret_cast<const char*>(glGetString(GL_VERSION))},{"epoch",r.terrainSceneEpoch}}.dump(2);
    }
    const auto rows=captureRows(o.performanceTrace+".publications.csv");std::map<std::string,Row> roots;unsigned invalid=0,obsolete=0,published=0;
    for(const auto& row:rows) if(row.at("kind")=="reload") {
        roots[row.at("attempt")]=row;EXPECT_EQ(row.at("capture_mode"),"1");
        if(row.at("outcome")=="invalid_config") ++invalid;
        else if(row.at("outcome")=="obsolete") {++obsolete;EXPECT_TRUE(row.at("publication_ms").empty());}
        else {EXPECT_EQ(row.at("outcome"),"published");++published;EXPECT_EQ(row.at("epoch"),"3");
            EXPECT_GE(std::stod(row.at("publication_ms")),std::stod(row.at("scene_exchange_ms")));}
    }
    EXPECT_EQ(invalid,1u);EXPECT_EQ(obsolete,1u);EXPECT_EQ(published,1u);
    if(mode!="cpu") {
        const auto work=captureRows(o.performanceTrace+".gpu-work.csv");unsigned offLive=0;
        std::map<std::string,std::set<std::string>> stages;
        for(const auto& row:work) if(row.at("attempt")!="0") {
            const auto child=std::find_if(rows.begin(),rows.end(),[&](const auto& c){return c.at("attempt")==row.at("attempt");});
            ASSERT_NE(child,rows.end());
            EXPECT_EQ(row.at("epoch"),child->at("epoch"));EXPECT_EQ(row.at("request_serial"),child->at("request_serial"));
            EXPECT_EQ(row.at("body"),child->at("body"));
            if(!child->at("prepared_ms").empty()) {
                EXPECT_TRUE((row.at("field")==child->at("land_field") && row.at("topology")==child->at("land_topology")) ||
                    (row.at("field")==child->at("water_field") && row.at("topology")==child->at("water_topology")));
            }
            if(row.at("frame").empty()) {
                ++offLive;EXPECT_FALSE(child->at("parent_attempt").empty());stages[row.at("attempt")].insert(row.at("stage"));
            }
        }
        EXPECT_GT(offLive,0u);
        for(const auto& row:rows) if(!row.at("parent_attempt").empty()) {
            EXPECT_TRUE(stages[row.at("attempt")].contains("terrain_field"));
            EXPECT_TRUE(stages[row.at("attempt")].contains("terrain_expansion"));
        }
    }
    for(const auto& [id,root]:roots) {
        unsigned children=0;for(const auto& row:rows) if(row.at("parent_attempt")==id) {
            ++children;EXPECT_EQ(row.at("epoch"),root.at("epoch"));EXPECT_EQ(row.at("outcome"),root.at("outcome"));
            if(root.at("outcome")=="published") EXPECT_EQ(row.at("end_frame"),root.at("end_frame"));
        }
        EXPECT_EQ(std::to_string(children),root.at("child_attempts"));
    }
}
}
TEST(TerrainFrame, CaptureAndSynchronousCpuReloadTraceCompleteConsumers) {captureCase("cpu");}
TEST(TerrainFrame, CaptureAndSynchronousLegacyComputeReloadTraceCompleteConsumers) {captureCase("legacy");}
TEST(TerrainFrame, CaptureAndSynchronousResidentReloadTraceCompleteConsumers) {captureCase("resident");}
