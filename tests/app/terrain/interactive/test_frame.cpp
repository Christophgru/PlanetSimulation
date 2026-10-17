#include "FrameTestSupport.h"
#include "rendering/geometry/terrain/detail/TerrainMorph.h"

TEST(TerrainFrame, CharacterRefreshesPlantedFeetForLivePhaseAfterRebindAndPreview) {
    auto o=options("character-phase");
    json scene;std::ifstream(o.configPath)>>scene;
    for(auto& p:scene["planets"]) {
        p["rotation"]={{"period_seconds",0},{"axial_tilt_deg",0}};
        p["atmosphere"]["enabled"]=false;p["water"]["enabled"]=false;
        p["foliage"]["enabled"]=false;
        p["terrain_landscape"]={{"enabled",false}};p["surface_noise"]=json::array();
    }
    auto& p=scene["planets"][0];
    p["surface_noise"]=json::array({{{"amplitude_m",.01},{"wavelength_m",.25},{"octaves",1},{"seed",-71}}});
    p["terrain_lod"].update({{"base_edge_segments",1},{"medium_edge_segments",4},{"max_edge_segments",8},
        {"steep_edge_segments",8},{"max_triangle_budget",100000},{"shoreline_edge_m",0},
        {"near_surface_distance_m",300},{"mid_surface_distance_m",1000},{"relief_sinking",true},
        {"geometric_error_m",.05},{"local_detail_streaming",true},{"local_detail_radius_m",1.5},
        {"local_edge_m",.05},{"local_error_m",.01},{"local_transition_m",.5}});
    std::ofstream(o.configPath)<<scene.dump(2);
    rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    r.preparePlanetMeshes(eye(r),false);
    ASSERT_TRUE(r.residentSceneReady());
    const auto selected=r.scene.scenario.surface_camera.planet_index;
    auto& mesh=r.meshes.planetMeshes[selected];ASSERT_TRUE(mesh.contacts);
    const auto radial=glm::normalize(r.scene.bodies[selected+1].toLocalPoint(eye(r)));
    const double scale=mesh.contacts->radiusMeters();
    // Synthetic CPU phase exercises the real character consumer without enabling
    // terrain deformation/grass at runtime. Actual GPU-plane parity is tested
    // by TerrainCompute's retained-target case; this test does not draw this mesh.
    auto phase=std::make_shared<rendering::TerrainMorphState>();mesh.morphState=phase;
    phase->set({radial,scale,0,.15});r.prepareAstronaut(0);
    const auto before=r.astronaut.motion.pose().feet;
    const auto previous=r.astronautGroundRevision;
    phase->set({radial,scale,1,.5});
    r.astronautGround.bind(mesh.contacts,mesh.revision,phase);
    const auto preview=r.previewAstronautEye(0,r.characterTerrainContacts());EXPECT_TRUE(std::isfinite(glm::length(preview)));
    EXPECT_EQ(r.astronautGroundRevision,previous);
    for(int leg=0;leg<2;++leg) EXPECT_EQ(r.astronaut.motion.pose().feet[leg].contact.position,before[leg].contact.position);
    r.prepareAstronaut(0);
    EXPECT_EQ(r.astronautGroundRevision,(rendering::SurfaceContact::Version{mesh.revision,phase->revision()}));
    EXPECT_EQ(r.astronautGroundRevision.mesh,previous.mesh);
    double changed=0;
    for(int leg=0;leg<2;++leg) {
        const auto expected=r.astronautGround.sample(before[leg].contact.position,[](const auto&) -> rendering::GroundContact {
            throw std::logic_error("Missing phase contact");
        });
        const auto& actual=r.astronaut.motion.pose().feet[leg];
        EXPECT_TRUE(actual.planted());EXPECT_EQ(actual.contact.position,expected.position);
        EXPECT_LT(glm::length(glm::normalize(actual.contact.position)-glm::normalize(before[leg].contact.position)),1e-12);
        changed=std::max(changed,glm::length(actual.contact.position-before[leg].contact.position));
    }
    EXPECT_GT(changed,.0001);
}

TEST(TerrainFrame, VerticalOnlyAdaptiveMovementRequestsAndPublishesCompleteGenerations) {
    for(const std::string mode:{"resident","cpu_async","cpu_capture"}) {
        SCOPED_TRACE(mode);
        const bool managed=mode=="resident",capture=mode=="cpu_capture";
        auto o=options("vertical-"+mode);o.thirdPersonRenderMode=false;
        if(!managed) {o.terrainBackend="cpu";o.terrainGrassPlanner="cpu";}
        json scene;std::ifstream(o.configPath)>>scene;
        for(auto& p:scene["planets"]) {
            p["rotation"]={{"period_seconds",0},{"axial_tilt_deg",0}};
            p["atmosphere"]["enabled"]=false;p["water"]["enabled"]=false;
            p["terrain_landscape"]={{"enabled",false}};p["surface_noise"]=json::array();
        }
        auto& p=scene["planets"][0];
        p["surface_noise"]=json::array({{{"amplitude_m",.01},{"wavelength_m",.25},{"octaves",1},{"seed",-71}}});
        p["terrain_lod"].update({{"base_edge_segments",1},{"medium_edge_segments",4},{"max_edge_segments",8},
            {"steep_edge_segments",8},{"max_triangle_budget",30000},{"shoreline_edge_m",0},
            {"near_surface_distance_m",35},{"mid_surface_distance_m",110},{"relief_sinking",true},
            {"geometric_error_m",.05},{"local_detail_streaming",true},{"local_detail_radius_m",.5},
            {"local_edge_m",.05},{"local_error_m",.01},{"local_transition_m",.5}});
        p["foliage"].update({{"density_per_m2",1},{"draw_distance_m",5},{"quad_distance_m",2}});
        std::ofstream(o.configPath)<<scene.dump(2);
        rendering::Renderer renderer(o);auto& r=Probe::state(renderer);r.options.renderTestMode=capture;
        std::unique_ptr<GlAudit> audit;if(managed) audit=std::make_unique<GlAudit>();
        const auto& body=r.scene.bodies[1];const auto radius=r.scene.scenario.planets[0].radius;
        const auto radial=glm::normalize(body.toLocalPoint(eye(r)));
        const auto up=body.orientation*radial;
        const auto pose=[&](double scale) {
            r.scene.surfaceCamera->followFlight(body.position+body.orientation*(radial*(scale*radius)),up,false);
        };
        const auto step=[&]{r.preparePlanetMeshes(eye(r),!capture);};
        const auto settled=[&] {
            return std::all_of(r.meshReady.begin(),r.meshReady.end(),[](bool ready){return ready;}) &&
                !r.terrainJobs.pending(r.terrainSceneEpoch) && (!managed || !r.terrainPublication->pending());
        };
        json trace=json::array();pose(1.5);
        ASSERT_TRUE(until(step,settled));draw(r,false);trace.push_back(r.terrainPublicationState());
        EXPECT_EQ(r.meshes.planetMeshes[0].terrainStats.localDetailTriangles,0);
        for(const double scale:{1.002,1.5}) {
            const auto revision=r.meshes.planetMeshes[0].revision;
            const auto submitted=r.terrainJobs.stats().submitted;
            pose(scale);step();
            // Pure radial movement must reach the real scheduling branch,
            // including pending-request coalescing, rather than grass only.
            if(capture) ASSERT_GT(r.meshes.planetMeshes[0].revision,revision);
            else ASSERT_GT(r.terrainJobs.stats().submitted,submitted);
            ASSERT_TRUE(until(step,[&]{return settled() && r.meshes.planetMeshes[0].revision>revision;}));
            EXPECT_FALSE(r.meshes.planetMeshes[0].terrainStats.parentReused);
            EXPECT_NEAR(glm::length(r.lastTerrainEyes[0]),scale*radius,1e-10*radius);
            const auto& stats=r.meshes.planetMeshes[0].terrainStats;
            if(scale<1.1) {EXPECT_GT(stats.localDetailTriangles,0);EXPECT_FALSE(stats.localDetailLimited);}
            else EXPECT_EQ(stats.localDetailTriangles,0);
            draw(r,false);trace.push_back(r.terrainPublicationState());
        }
        if(managed) save(o,trace,*audit,r);
    }
}

TEST(TerrainFrame, StartupGrassOnlyRecoveryAndProgrammaticWalkingPublishWithoutWaits) {
    auto o=options("movement");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    // Exercise the real frame preparation branch without opening the public CLI gate.
    r.options.renderTestMode=false;GlAudit audit;json trace=json::array();
    EXPECT_FALSE(r.residentSceneReady());EXPECT_FALSE(r.astronaut.motion.ready());
    tick(r);EXPECT_FALSE(r.residentSceneReady());EXPECT_FALSE(r.astronaut.motion.ready());
    ASSERT_TRUE(until([&]{tick(r);},[&]{return r.residentSceneReady();}));
    for(const auto& mesh:r.meshes.planetMeshes) {
        EXPECT_TRUE(mesh.residentTerrain);EXPECT_TRUE(mesh.contacts);EXPECT_TRUE(mesh.vertices.empty());EXPECT_TRUE(mesh.indices.empty());
    }
    draw(r);trace.push_back(r.terrainPublicationState());
    const auto revision=r.meshes.planetMeshes[0].revision;
    ASSERT_TRUE(until([&]{tick(r);},[&]{return idle(r) && !r.terrainPublication->retiring(0) && !r.terrainPublication->retiring(1);}));
    const auto contacts=r.meshes.planetMeshes[0].contacts;
    const auto anchor=r.terrainPublication->installed(0).grassEye;
    const auto grassPublished=r.terrainPublication->stats().grassOnlyPublished;
    r.scene.surfaceCamera->walk(1,0,1/(r.scene.surfaceCamera->walkSpeed()*r.scene.scenario.metersPerWorldUnit()));
    delay=true;ASSERT_TRUE(until([&]{tick(r);},[&]{return r.residentStage.has_value();}));
    ASSERT_TRUE(r.residentStageGrassOnly);failPoll=true;tick(r);
    EXPECT_EQ(r.terrainPublication->installed(0).grassEye,anchor);EXPECT_EQ(r.meshes.planetMeshes[0].contacts,contacts);
    EXPECT_EQ(r.meshes.planetMeshes[0].revision,revision);draw(r);trace.push_back(r.terrainPublicationState());
    delay=false;ASSERT_TRUE(until([&]{tick(r);},[&]{return idle(r) && r.terrainPublication->stats().grassOnlyPublished>grassPublished;}));
    EXPECT_EQ(r.meshes.planetMeshes[0].revision,revision);EXPECT_EQ(r.meshes.planetMeshes[0].contacts,contacts);
    draw(r);trace.push_back(r.terrainPublicationState());
    // Ground camera steps represent 6 m/s walking then 12 m/s sprinting.
    for(int frame=0;frame<18;++frame) {
        const double meters=frame<6 ? .5 : 1;
        r.scene.surfaceCamera->walk(1,0,meters/(r.scene.surfaceCamera->walkSpeed()*r.scene.scenario.metersPerWorldUnit()));
        r.characterWindTime+=(1.0/12);r.preparePlanetMeshes(eye(r),true,1.0/12);r.prepareAstronaut(1.0/12);
    }
    ASSERT_TRUE(until([&]{tick(r);},[&]{return idle(r);}));draw(r);trace.push_back(r.terrainPublicationState());
    EXPECT_GT(r.meshes.planetMeshes[0].revision,revision);EXPECT_GT(r.characterPreviews,0u);
    EXPECT_GT(r.astronautState()["walked_m"].get<double>(),10);
    EXPECT_FALSE(r.plannedCharacterEye);save(o,trace,audit,r);
}

TEST(TerrainFrame, DelayedGpuKeepsBoundedCpuCompletionAndRetainsDrawableRetiredBuffers) {
    auto o=options("delayed");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    r.options.renderTestMode=false;GlAudit audit;json trace=json::array();
    delay=true;
    ASSERT_TRUE(until([&]{tick(r);},[&]{return r.residentStage.has_value() && r.terrainJobs.stats().ready==1;}));
    const auto ready=r.terrainJobs.readyIdentity();ASSERT_TRUE(ready);
    for(int i=0;i<20;++i) {tick(r);EXPECT_EQ(r.terrainJobs.readyIdentity(),ready);EXPECT_FALSE(r.residentSceneReady());}
    delay=false;ASSERT_TRUE(until([&]{tick(r);},[&]{return idle(r);}));draw(r);trace.push_back(r.terrainPublicationState());
    // Drain bootstrap grass retirement before delaying the next preparation;
    // otherwise its bounded spare correctly prevents that submission.
    ASSERT_TRUE(until([&]{tick(r);},[&]{return idle(r) && r.terrainPublication->canSubmit(0);}));
    const auto oldBuffer=r.meshes.planetMeshes[0].vbo;const auto oldRevision=r.meshes.planetMeshes[0].revision;
    r.scene.surfaceCamera->walk(1,0,15/(r.scene.surfaceCamera->walkSpeed()*r.scene.scenario.metersPerWorldUnit()));
    // Independent grass can win the submission slot while the CPU terrain job
    // finishes. Select the full terrain stage before holding its fence, rather
    // than accidentally holding a grass-only stage and starving this test.
    ASSERT_TRUE(until([&]{tick(r);},[&]{return r.residentStage && !r.residentStageGrassOnly;}));
    delay=true;
    for(int i=0;i<10;++i) tick(r);
    EXPECT_EQ(r.meshes.planetMeshes[0].revision,oldRevision);draw(r);trace.push_back(r.terrainPublicationState());
    // Complete the real preparation, then hold only the newly created retirement.
    delay=false;ASSERT_TRUE(until([&]{tick(r);},[&]{return r.meshes.planetMeshes[0].revision>oldRevision;}));
    ASSERT_TRUE(r.terrainPublication->retiring(0));delay=true;
    for(int i=0;i<10;++i) tick(r);
    EXPECT_TRUE(glIsBuffer(oldBuffer));draw(r);trace.push_back(r.terrainPublicationState());
    failPoll=true;tick(r);EXPECT_TRUE(r.residentRetirementFailed);EXPECT_TRUE(glIsBuffer(oldBuffer));
    for(int i=0;i<5;++i) tick(r);
    delay=false;ASSERT_TRUE(until([&]{tick(r);},[&]{return !r.terrainPublication->retiring(0) && idle(r);}));
    EXPECT_FALSE(glIsBuffer(oldBuffer));draw(r);trace.push_back(r.terrainPublicationState());save(o,trace,audit,r);
}

TEST(TerrainFrame, FailedFenceBacksOffAndStaleMaskCompletionCannotReplaceLiveGeneration) {
    auto o=options("recovery");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    r.options.renderTestMode=false;GlAudit audit;json trace=json::array();
    ASSERT_TRUE(until([&]{tick(r);},[&]{return idle(r);}));draw(r);trace.push_back(r.terrainPublicationState());
    const auto buffer=r.meshes.planetMeshes[0].vbo;const auto revision=r.meshes.planetMeshes[0].revision;
    const auto home=eye(r);const auto& body=r.scene.bodies[1];
    const auto far=body.position+body.orientation*(glm::normalize(body.toLocalPoint(home))*4.0*r.scene.scenario.planets[0].radius);
    const auto farTick=[&]{r.preparePlanetMeshes(far,true);};
    failFence=true;ASSERT_TRUE(until(farTick,[&]{return r.residentFrameFailures==1;}));
    const auto submitted=r.terrainJobs.stats().submitted;
    for(int i=0;i<40;++i) farTick();
    EXPECT_EQ(r.terrainJobs.stats().submitted,submitted);EXPECT_EQ(r.meshes.planetMeshes[0].vbo,buffer);
    EXPECT_EQ(r.meshes.planetMeshes[0].revision,revision);draw(r);trace.push_back(r.terrainPublicationState());
    delay=true;ASSERT_TRUE(until(farTick,[&]{return r.residentStage.has_value();}));
    const auto rejected=r.terrainRejectedBuilds;tick(r);
    EXPECT_TRUE(!r.residentStage || r.residentStageGrassOnly);
    EXPECT_GT(r.terrainRejectedBuilds,rejected);EXPECT_EQ(r.meshes.planetMeshes[0].vbo,buffer);
    delay=false;ASSERT_TRUE(until(farTick,[&]{return r.lastLocalMask[0]==0 && !r.residentStage;}));
    EXPECT_GT(r.meshes.planetMeshes[0].revision,revision);draw(r);trace.push_back(r.terrainPublicationState());
    ASSERT_TRUE(until([&]{tick(r);},[&]{return idle(r) && r.lastLocalMask[0]==1;}));
    draw(r);trace.push_back(r.terrainPublicationState());EXPECT_FALSE(r.terrainFailures[0]);save(o,trace,audit,r);
}
