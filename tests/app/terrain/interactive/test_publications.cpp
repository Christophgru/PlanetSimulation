#include "FrameTestSupport.h"
#include <map>
#include <sstream>

namespace {
using Row=std::map<std::string,std::string>;
std::vector<Row> publicationRows(const std::string& path) {
    const auto split=[](const std::string& s) {
        std::vector<std::string> values;std::istringstream input(s);std::string value;
        while(std::getline(input,value,',')) values.push_back(value);
        if(!s.empty() && s.back()==',') values.emplace_back();return values;
    };
    std::ifstream input(path);std::string line;std::getline(input,line);const auto header=split(line);
    std::vector<Row> rows;
    while(std::getline(input,line)) {
        const auto values=split(line);EXPECT_EQ(values.size(),header.size());if(values.size()!=header.size()) continue;
        Row row;for(std::size_t i=0;i<header.size();++i) row[header[i]]=values[i];rows.push_back(row);
    }
    return rows;
}
struct Receipt {
    std::uint64_t epoch,body,serial,frame;
    rendering::PublicationProfiler::Generation generation;
};
void runPublicationCase(bool compute) {
    auto o=options(compute ? "publication-compute" : "publication-cpu");
    if(!compute) {o.terrainBackend="cpu";o.terrainGrassPlanner="cpu";}
    o.performanceTrace=o.configPath+".frames.csv";std::vector<Receipt> receipts;
    {
        rendering::Renderer renderer(o);auto& r=Probe::state(renderer);r.options.renderTestMode=false;
        std::optional<GlAudit> audit;if(compute) audit.emplace();
        auto& p=r.profiler.publications();
        const auto tickFrame=[&] {r.profiler.beginFrame(0);tick(r);r.profiler.endFrame();glFlush();};
        const auto drawFrame=[&] {
            const auto frame=r.profiler.nextFrameNumber();r.profiler.beginFrame(0);draw(r);
            for(std::size_t i=0;i<r.meshReady.size();++i) {
                const auto& land=r.meshes.planetMeshes[i];const auto& water=r.meshes.waterMeshes[i];
                const auto& l=land.terrainStats.generation;const auto& w=water.terrainStats.generation;
                rendering::PublicationProfiler::Generation g{l.field,l.topology,w.field,w.topology,land.revision,water.revision};
                if(compute) {const auto eye=r.terrainPublication->installed(i).grassEye;g.grassEye={eye.x,eye.y,eye.z};}
                receipts.push_back({r.terrainSceneEpoch,i,r.installedTerrainSerial[i],frame,g});
            }
            r.profiler.endFrame();glFlush();p.collect();
        };
        ASSERT_TRUE(until(tickFrame,[&]{return r.residentSceneReady();}));p.collect();
        const auto before=publicationRows(o.performanceTrace+".publications.csv");
        for(const auto& row:before) EXPECT_NE(row.at("outcome"),"published");
        EXPECT_GT(p.stats().active,0u);drawFrame();
        ASSERT_TRUE(until(tickFrame,[&]{return !compute || (idle(r) && r.terrainPublication->canSubmit(0));}));drawFrame();
        const auto originalSerial=r.installedTerrainSerial[0],originalRevision=r.meshes.planetMeshes[0].revision;
        if(compute) {
            r.scene.surfaceCamera->walk(1,0,1/(r.scene.surfaceCamera->walkSpeed()*r.scene.scenario.metersPerWorldUnit()));
            delay=true;ASSERT_TRUE(until(tickFrame,[&]{return r.residentStage.has_value();}));ASSERT_TRUE(r.residentStageGrassOnly);
            failPoll=true;tickFrame();drawFrame();delay=false;
            ASSERT_TRUE(until(tickFrame,[&]{return idle(r);}));drawFrame();
            EXPECT_EQ(r.installedTerrainSerial[0],originalSerial);EXPECT_EQ(r.meshes.planetMeshes[0].revision,originalRevision);
        }
        r.scene.surfaceCamera->walk(1,0,15/(r.scene.surfaceCamera->walkSpeed()*r.scene.scenario.metersPerWorldUnit()));
        ASSERT_TRUE(until(tickFrame,[&]{return r.meshes.planetMeshes[0].revision>originalRevision && (!compute || idle(r));}));drawFrame();
        EXPECT_GT(r.installedTerrainSerial[0],originalSerial);EXPECT_EQ(p.stats().dropped,0u);EXPECT_LE(p.stats().peakActive,rendering::PublicationProfiler::capacity);
        json result{{"renderer",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
            {"version",reinterpret_cast<const char*>(glGetString(GL_VERSION))},{"draws",json::array()},
            {"peak_active",p.stats().peakActive},{"dropped",p.stats().dropped}};
        for(const auto& receipt:receipts) result["draws"].push_back({{"epoch",receipt.epoch},{"body",receipt.body},
            {"serial",receipt.serial},{"frame",receipt.frame},{"land_field",std::to_string(receipt.generation.landField)},
            {"land_topology",std::to_string(receipt.generation.landTopology)},{"land_revision",receipt.generation.landRevision},
            {"water_field",std::to_string(receipt.generation.waterField)},{"water_topology",std::to_string(receipt.generation.waterTopology)},
            {"water_revision",receipt.generation.waterRevision},{"grass_eye",receipt.generation.grassEye}});
        if(audit) result["gl"]=audit->state();std::ofstream(o.configPath+".publication.json")<<result.dump(2);
    }
    const auto rows=publicationRows(o.performanceTrace+".publications.csv");unsigned published=0,grass=0,failures=0;
    std::map<std::uint64_t,Row> attempts;
    for(const auto& row:rows) {
        const auto attempt=std::stoull(row.at("attempt"));ASSERT_GT(attempt,0u);EXPECT_TRUE(attempts.emplace(attempt,row).second);
        EXPECT_NE(row.at("outcome"),"trace_overflow");
        if(row.at("outcome")=="preparation_failed") ++failures;
        if(row.at("outcome")!="published") {EXPECT_TRUE(row.at("publication_ms").empty());continue;}
        ++published;if(row.at("kind")=="grass") ++grass;
        EXPECT_GE(std::stod(row.at("publication_ms")),std::stod(row.at("prepared_ms")));
        EXPECT_FALSE(row.at("publication_frames").empty());
        bool matched=false;
        for(const auto& receipt:receipts) {
            const auto& g=receipt.generation;
            if(row.at("epoch")!=std::to_string(receipt.epoch) || row.at("body")!=std::to_string(receipt.body) ||
               row.at("request_serial")!=std::to_string(receipt.serial) || row.at("end_frame")!=std::to_string(receipt.frame) ||
               row.at("land_field")!=std::to_string(g.landField) || row.at("land_topology")!=std::to_string(g.landTopology) ||
               row.at("land_revision")!=std::to_string(g.landRevision) || row.at("water_field")!=std::to_string(g.waterField) ||
               row.at("water_topology")!=std::to_string(g.waterTopology) || row.at("water_revision")!=std::to_string(g.waterRevision)) continue;
            matched=true;for(int i=0;i<3;++i) {
                const auto& eye=row.at(std::string("grass_eye_")+"xyz"[i]);
                if(compute) EXPECT_DOUBLE_EQ(std::stod(eye),g.grassEye[i]);else EXPECT_TRUE(eye.empty());
            }
            break;
        }
        EXPECT_TRUE(matched) << "No actual draw matches attempt " << attempt;
    }
    EXPECT_GE(published,3u);
    if(compute) {
        EXPECT_GT(grass,0u);EXPECT_GT(failures,0u);
        const auto work=publicationRows(o.performanceTrace+".gpu-work.csv");unsigned joined=0;
        for(const auto& row:work) if(row.at("attempt")!="0") {
            const auto found=attempts.find(std::stoull(row.at("attempt")));ASSERT_NE(found,attempts.end());
            EXPECT_EQ(row.at("epoch"),found->second.at("epoch"));EXPECT_EQ(row.at("request_serial"),found->second.at("request_serial"));
            EXPECT_EQ(row.at("body"),found->second.at("body"));++joined;
        }
        EXPECT_GT(joined,0u);
    } else EXPECT_EQ(grass,0u);
}
}
TEST(TerrainFrame, PublicationTraceMatchesComputeDrawsAndGrassFailureRecovery) {runPublicationCase(true);}
TEST(TerrainFrame, PublicationTraceMatchesDefaultCpuStartupAndWorkerReplacement) {runPublicationCase(false);}
