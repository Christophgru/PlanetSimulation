#include "rendering/runtime/RendererState.h"
#include "rendering/runtime/terrain/reload/ReloadCommit.h"
#include <GLFW/glfw3.h>
namespace rendering {
ReloadCommitState prepareReloadCommit(const app::PreparedScene& scene,bool third,double time,
    std::optional<glm::dvec3> eye) {
    ReloadCommitState c;c.time=time;c.characterEye=eye;
    c.walkSpeed=6.0/scene.scenario.metersPerWorldUnit();
    if(!std::isfinite(c.walkSpeed) || c.walkSpeed<=0) throw std::invalid_argument("Invalid reload walking speed");
    if(scene.surfaceCamera) c.clip=surfaceClipPlanes(
        third ? .2/scene.scenario.metersPerWorldUnit() : scene.surfaceCamera->configuredClearance(),
        glm::length((eye ? *eye : scene.surfaceCamera->position())-scene.sunPosition),scene.scenario.sun.radius);
    c.wall=glfwGetTime();if(!std::isfinite(c.wall)) throw std::runtime_error("Invalid reload wall clock");
    return c;
}
void Renderer::Impl::finishSceneReload(ReloadTracking& tracking,app::CommandLineOptions& nextOptions,
    nlohmann::json& nextReplay,const ReloadCommitState& commit) {
        // No allocations after exchange. Optional cameras keep their addresses.
        tracking.exchange(*this);using std::swap;swap(options,nextOptions);source.replayDocument.swap(nextReplay);
        profiler.publications().discardOlderEpochs(terrainSceneEpoch);
        terrainJobs.advanceEpoch(terrainSceneEpoch);simulationTime=commit.time;simulationClock.reset(commit.time,commit.wall);characterWindTime=commit.time;surfaceClip=commit.clip;
        cameraInput.rebind(scene.surfaceCamera ? &*scene.surfaceCamera : nullptr,scene.planetOrbitCamera ? &*scene.planetOrbitCamera : nullptr);
        cameraInput.setThirdPersonWalkSpeed(commit.walkSpeed);
        if(options.renderTestMode) {
            cameraInput.selectOrbit(); // Selecting an orbit must not realign the staged camera from an old walking mode.
            if(options.thirdPersonRenderMode) cameraInput.selectThirdPerson();else if(options.surfaceRenderMode) cameraInput.selectSurface();
            else if(options.planetRenderMode) cameraInput.selectPlanetOrbit();else cameraInput.selectOrbit();
        }
        astronaut.motion=AstronautMotion{};astronaut.exhaust.clear();astronaut.lastEmitter.reset();astronaut.exhaustTime.reset();
        astronautGround.clear();astronautGroundRevision=0;astronautReplayRestored=false;astronautBenchmarkBoost=false;
        astronautFlightControl={0,0};inputContext.spacePresses=0;plannedCharacterEye.reset();
        reloadCharacterEye=commit.characterEye;reloadCharacterPending=bool(commit.characterEye) && options.renderTestMode;
        if(scene.surfaceCamera) {
            const auto i=scene.scenario.surface_camera.planet_index;captureTerrainEye=lastTerrainEyes[i];astronaut.planetIndex=i;
            if(meshes.planetMeshes[i].contacts) astronautGround.bind(meshes.planetMeshes[i].contacts,meshes.planetMeshes[i].revision);
            else astronautGround.bind(meshes.planetMeshes[i].vertices,meshes.planetMeshes[i].indices,meshes.planetMeshes[i].revision,
                scene.scenario.planets[i].radius*scene.scenario.metersPerWorldUnit());
        }
        terrainShadows.destroy();waterReflection.destroy();frameReuse.invalidate();
        orbitTrails.clear();orbitColors.clear();orbitColorRevisions.clear();orbitTrailEpoch=std::numeric_limits<double>::quiet_NaN();
        ++sceneReloads;glFlush();
        residentStage.reset();residentStageZones.clear();residentStageGrassOnly=false;
        residentRetryAfter=0;residentRetirementFailed=false;
}
}
