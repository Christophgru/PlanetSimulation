#include "FrameTestSupport.h"

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
    delay=true;ASSERT_TRUE(until([&]{tick(r);},[&]{return r.residentStage && !r.residentStageGrassOnly;}));
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
