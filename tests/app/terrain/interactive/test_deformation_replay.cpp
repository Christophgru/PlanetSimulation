#include "FrameTestSupport.h"
#include "rendering/geometry/terrain/detail/TerrainMorphReplay.h"
#include <iterator>

namespace {
json replayJson(const std::string& path) {json result;std::ifstream(path)>>result;return result;}
std::vector<char> replayBytes(const std::string& path) {
    std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};
}
void microReplayScene(const app::CommandLineOptions& o) {
    auto scene=replayJson(o.configPath);
    for(auto& p:scene["planets"]) {
        p["rotation"]={{"period_seconds",0},{"axial_tilt_deg",0}};
        p["atmosphere"]["enabled"]=false;p["water"]["enabled"]=false;
        p["foliage"]["enabled"]=false;p["terrain_landscape"]={{"enabled",false}};p["surface_noise"]=json::array();
    }
    auto& p=scene["planets"][0];
    p["surface_noise"]=json::array({{{"amplitude_m",.01},{"wavelength_m",.25},{"octaves",1},{"seed",-71}}});
    p["terrain_lod"].update({{"base_edge_segments",1},{"medium_edge_segments",4},{"max_edge_segments",8},
        {"steep_edge_segments",8},{"max_triangle_budget",100000},{"shoreline_edge_m",0},
        {"near_surface_distance_m",300},{"mid_surface_distance_m",1000},{"relief_sinking",true},
        {"geometric_error_m",.05},{"local_detail_streaming",true},{"local_detail_radius_m",1.5},
        {"local_edge_m",.05},{"local_error_m",.01},{"local_transition_m",.5}});
    p["foliage"].update({{"enabled",true},{"density_per_m2",1},{"max_blades",100000},
        {"draw_distance_m",5},{"quad_distance_m",2},{"frustum_culling",false}});
    scene["surface_camera"]["altitude"]=.002;
    std::ofstream(o.configPath)<<scene.dump(2);
}
}

TEST(TerrainFrame, DeformationSnapshotValidatesBodyScaleAndSchemaBeforeRendererCreation) {
    auto o=options("phase-validation");o.thirdPersonRenderMode=false;o.terrainBackend=o.terrainGrassPlanner="cpu";
    const auto scene=replayJson(o.configPath);const config::ScenarioConfig parsed{config::Config(json(scene))};
    const double scale=parsed.planets[0].radius*parsed.metersPerWorldUnit();
    rendering::TerrainMorphReplay record{{123,456,rendering::PlanetField::version,4,rendering::TerrainBackend::Cpu},scale,
        rendering::TerrainMorphFootprint{{0,0,1},scale,.2,.4}};
    auto entry=record.json();entry["body"]=parsed.planets[0].name;
    auto records=json::array();for(std::size_t i=0;i<parsed.planets.size();++i) records.push_back(i?json(nullptr):entry);
    const json valid{{"scenario",scene},{"surface_camera",scene["surface_camera"]},
        {"render",{{"width",320},{"height",180},{"terrain_backend","cpu"},{"terrain_deformation",records}}}};
    o.replayPath=o.configPath+".replay.json";
    const auto save=[&](const json& j){std::ofstream(o.replayPath)<<j.dump(2);};save(valid);
    EXPECT_NO_THROW(app::SceneSource source(o));
    for(int problem=0;problem<5;++problem) {
        auto bad=valid;
        if(problem==0) bad["render"]["terrain_deformation"]=json::array();
        if(problem==1) bad["render"]["terrain_deformation"][0]["body"]="wrong";
        if(problem==2) bad["render"]["terrain_deformation"][0]["meters_per_radius"]=scale+1;
        if(problem==3) bad["render"]["terrain_deformation"][0]["version"]=2;
        if(problem==4) bad["render"]["terrain_deformation"][0]["footprint"]["eye_radial"]={0,0,2};
        save(bad);EXPECT_THROW(app::SceneSource source(o),std::invalid_argument)<<problem;
    }
    save(valid);o.renderTestMode=false;EXPECT_THROW(app::SceneSource source(o),std::invalid_argument);
}

TEST(TerrainFrame, OriginalAndActiveDeformationCapturesReplayAndReloadExactlyOnBothBackends) {
    auto baseline=options("phase-reference");baseline.thirdPersonRenderMode=false;
    baseline.terrainBackend=baseline.terrainGrassPlanner="cpu";microReplayScene(baseline);
    json reference;rendering::TerrainGenerationKey key;double scale=0;glm::dvec3 radial;
    {
        rendering::Renderer renderer(baseline);ASSERT_EQ(renderer.run(),0);auto& r=Probe::state(renderer);
        reference=replayJson(baseline.outputImagePath+".json");key=r.meshes.planetMeshes[0].terrainStats.generation;
        ASSERT_EQ(key.topologyVersion,4u);scale=r.scene.scenario.planets[0].radius*r.scene.scenario.metersPerWorldUnit();
        radial=glm::normalize(r.lastTerrainEyes[0]);
    }
    for(const std::string mode:{"cpu","resident","mirror","character"}) for(const bool active:{false,true}) {
        SCOPED_TRACE(mode);
        SCOPED_TRACE(active);
        const auto tag="phase-"+mode+(active?"-active":"-original");auto o=options(tag);o.thirdPersonRenderMode=mode=="character";
        o.terrainBackend=mode=="cpu"?"cpu":"compute";o.terrainGrassPlanner=(mode=="resident" || mode=="character")?"gpu-v1":"cpu";
        auto saved=reference;
        rendering::TerrainMorphReplay record{key,scale,active?std::optional(rendering::TerrainMorphFootprint{radial,scale,.2,.4}):std::nullopt};
        auto entry=record.json();entry["body"]=saved["scenario"]["planets"][0]["name"];
        auto entries=json::array();for(std::size_t i=0;i<saved["scenario"]["planets"].size();++i) entries.push_back(i?json(nullptr):entry);
        saved["render"]["terrain_deformation"]=entries;saved["render"]["terrain_backend"]=o.terrainBackend;
        saved["render"]["terrain_grass_planner"]=o.terrainGrassPlanner;
        o.replayPath=o.configPath+".replay.json";std::ofstream(o.replayPath)<<saved.dump(2);
        std::vector<char> expected;
        {
            rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);auto& r=Probe::state(renderer);
            const auto& mesh=r.meshes.planetMeshes[0];EXPECT_TRUE(mesh.morphSource());
            if(mode=="cpu") {ASSERT_TRUE(mesh.morphData);EXPECT_EQ(mesh.morphData->phase,record.phase);}
            else {ASSERT_TRUE(mesh.morphState);EXPECT_EQ(mesh.deformationRevision(),active?1u:0u);EXPECT_TRUE(mesh.residentTerrain);}
            EXPECT_EQ(replayJson(o.outputImagePath+".json")["render"]["terrain_deformation"],entries);
            expected=replayBytes(o.outputImagePath);ASSERT_FALSE(expected.empty());
            if(mode=="resident" && active) {
                auto stale=saved;stale["render"]["terrain_deformation"][0]["topology"]=key.topology==1?"2":"1";
                std::ofstream(o.replayPath)<<stale.dump(2);
                const auto epoch=r.terrainSceneEpoch;const auto buffer=mesh.vbo;const auto state=mesh.morphState;
                EXPECT_THROW(renderer.reload(),std::invalid_argument);EXPECT_EQ(r.terrainSceneEpoch,epoch);
                EXPECT_EQ(r.meshes.planetMeshes[0].vbo,buffer);EXPECT_EQ(r.meshes.planetMeshes[0].morphState,state);
                std::ofstream(o.replayPath)<<saved.dump(2);
            }
            renderer.reload();ASSERT_EQ(renderer.run(),0);
            EXPECT_EQ(replayBytes(o.outputImagePath),expected);
            EXPECT_EQ(replayJson(o.outputImagePath+".json")["render"]["terrain_deformation"],entries);
            EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
        }
        auto again=options(tag+"-roundtrip");again.thirdPersonRenderMode=mode=="character";
        again.terrainBackend=again.terrainGrassPlanner="cpu";again.replayPath=o.outputImagePath+".json";
        rendering::Renderer renderer(again);ASSERT_EQ(renderer.run(),0);
        EXPECT_EQ(replayBytes(again.outputImagePath),expected);
        EXPECT_EQ(replayJson(again.outputImagePath+".json")["render"]["terrain_deformation"],entries);
    }
}
