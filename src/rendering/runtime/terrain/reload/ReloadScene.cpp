#include "rendering/runtime/RendererState.h"
#include "rendering/runtime/terrain/reload/ReloadCommit.h"
#include "rendering/runtime/terrain/reload/ReplayEffects.h"
#include "rendering/quality/OfflineQuality.h"
#include "config/SceneReplay.h"
#include <GLFW/glfw3.h>
#include <type_traits>

namespace rendering {
namespace {
glm::dvec3 vector(const nlohmann::json& j) {
    if(!j.is_array() || j.size()!=3) throw std::invalid_argument("Reload planning eye needs three values");
    const glm::dvec3 v{j.at(0).get<double>(),j.at(1).get<double>(),j.at(2).get<double>()};
    if(!std::isfinite(glm::length(v))) throw std::invalid_argument("Reload planning eye must be finite");
    return v;
}
}
void Renderer::Impl::retireSceneReload(bool captureWait) {
    if(retiredResidentScene) {
        if(captureWait) retiredResidentScene->waitRetiredForCapture();
        if(retiredResidentScene->pollRetired()) retiredResidentScene.reset();
    }
    if(retiredLegacyScene) {
        if(captureWait) retiredLegacyScene->waitForCapture();
        if(retiredLegacyScene->pollRetired()) retiredLegacyScene.reset();
    }
}
void Renderer::Impl::reloadScene() {
    if(terrainPublication && !options.renderTestMode) {requestResidentReload();return;}
    CpuTrace::Scope trace("scene.reload_transaction");
    try {
        retireSceneReload(options.renderTestMode);
        if(retiredResidentScene || retiredLegacyScene) throw std::runtime_error("Previous scene is still retiring");
        auto nextOptions=options;app::SceneSource nextSource(nextOptions);
        if(!terrainFallback.empty() && !nextOptions.lockedTerrainBackend) {
            nextOptions.terrainBackend="cpu";nextOptions.terrainGrassPlanner="cpu";
        }
        if(nextOptions.terrainBackend!=options.terrainBackend || nextOptions.terrainGrassPlanner!=options.terrainGrassPlanner ||
           nextOptions.renderTestWidth!=options.renderTestWidth || nextOptions.renderTestHeight!=options.renderTestHeight ||
           nextOptions.offlineQuality!=options.offlineQuality || nextOptions.atmosphereFullResolution!=options.atmosphereFullResolution ||
           nextOptions.lensFlare!=options.lensFlare)
            throw std::invalid_argument("Reload requires unchanged renderer pipeline and capture dimensions");
        app::PreparedScene prepared(offlineScenario(config::ScenarioConfig{config::Config{nlohmann::json(nextSource.document)}},nextOptions));
        const double time=config::replayStartTime(prepared.scenario,nextOptions.commandLineTime);
        prepared.updateSimulation(time);
        if((nextOptions.surfaceRenderMode || nextOptions.thirdPersonRenderMode) && !prepared.surfaceCamera)
            throw std::invalid_argument("Capture reload requires the configured surface camera");
        if(nextOptions.planetRenderMode && !prepared.planetOrbitCamera)
            throw std::invalid_argument("Capture reload requires a planet orbit camera");
        const auto count=prepared.scenario.planets.size();ReloadTracking tracking(count);
        const auto epoch=terrainSceneEpoch+1;
        if(!epoch) throw std::overflow_error("Scene epoch exhausted");
        const auto mode=options.renderTestMode ? (nextOptions.thirdPersonRenderMode ? CameraMode::ThirdPerson :
            nextOptions.surfaceRenderMode ? CameraMode::Surface : nextOptions.planetRenderMode ? CameraMode::PlanetOrbit : CameraMode::Orbit) : cameraInput.mode();
        const bool third=mode==CameraMode::ThirdPerson && prepared.surfaceCamera;
        const auto terrainEye=(mode==CameraMode::Surface || third) && prepared.surfaceCamera ? prepared.surfaceCamera->position() :
            mode==CameraMode::PlanetOrbit && prepared.planetOrbitCamera ? glm::dvec3(prepared.planetOrbitCamera->position) : glm::dvec3(prepared.sunCamera.position);
        const auto selected=prepared.scenario.surface_camera.planet_index;
        const auto& replay=nextSource.replayDocument;
        const nlohmann::json* pose=third && replay.contains("astronaut_pose") ? &replay.at("astronaut_pose") : nullptr;
        if(pose) characterReplay::validateEffects(*pose);
        std::optional<glm::dvec3> savedGrass;
        if(pose && pose->contains("grass_plan_eye")) savedGrass=vector(pose->at("grass_plan_eye"));
        std::unique_ptr<SceneTerrainReplacement> resident;
        std::unique_ptr<LegacySceneReplacement> legacy;
        if(terrainPublication) {
            if(!options.renderTestMode) throw std::logic_error("Interactive compute reload remains gated");
            resident=std::make_unique<SceneTerrainReplacement>(nextSource.document,std::move(prepared),
                SceneTerrainDestination{scene,source.document,meshes.planetMeshes,meshes.waterMeshes,grass.procedural,*terrainPublication,terrainSceneEpoch},epoch,time);
        } else legacy=std::make_unique<LegacySceneReplacement>(nextSource.document,std::move(prepared));
        const auto& stagedScene=resident ? resident->scene() : legacy->scene;
        std::vector<std::optional<TerrainCpuBuild>> builds(count);
        std::vector<TerrainBuildIdentity> identities(count);
        SceneMeshes previewMeshes(count);
        for(std::size_t i=0;i<count;++i) {
            const auto& planet=stagedScene.scenario.planets[i];auto eye=stagedScene.bodies[i+1].toLocalPoint(terrainEye);
            std::vector<int> zones;
            if(pose) {
                if(pose->contains("terrain_plan_eyes_world_units")) {
                    const auto& eyes=pose->at("terrain_plan_eyes_world_units");
                    if(!eyes.is_array() || eyes.size()!=count) throw std::invalid_argument("Reload replay needs one terrain anchor per body");
                    eye=vector(eyes.at(i));
                } else if(i==selected && pose->contains("terrain_plan_eye_world_units")) eye=vector(pose->at("terrain_plan_eye_world_units"));
                if(pose->contains("terrain_face_zones")) {
                    const auto& all=pose->at("terrain_face_zones");
                    if(!all.is_array() || all.size()!=count) throw std::invalid_argument("Reload replay needs terrain zones for every body");
                    zones=all.at(i).get<std::vector<int>>();
                }
            }
            TerrainBuildIdentity k;k.epoch=epoch;k.serial=++terrainRequestSerial;k.bodyIndex=i;k.bodyName=planet.name;
            k.field=stagedScene.terrainSurfaces[i].field().fingerprint();k.eye=eye;k.localMask=glm::length(eye)<3*planet.radius ? 1 : 0;
            k.backend=terrainCompute ? TerrainBackend::Compute : TerrainBackend::Cpu;k.resident=bool(resident);
            auto request=resident ? resident->requestLocal(i,eye,k.serial,std::move(zones)) :
                TerrainBuildRequest{k,stagedScene.terrainSurfaces[i],planet,std::move(zones),stagedScene.scenario.metersPerWorldUnit()};
            k=request.identity;identities[i]=k;
            auto built=terrainCompute ? terrainJobs.executeForReload(std::move(request)).take() : buildTerrainCpu(request);
            tracking.record(i,built,k);
            if(resident) {
                previewMeshes.planetMeshes[i].contacts=built.contacts;previewMeshes.planetMeshes[i].revision=1;
                builds[i]=std::move(built);
            } else {
                if(terrainCompute) {
                    TerrainGpuPreparation stage(std::move(built),k,planet,*terrainCompute);stage.waitForCapture();
                    built=std::move(stage.cpu);
                    legacy->meshes.planetMeshes[i].loadComputedTerrain(std::move(built.geometry),*stage.land,true);
                    if(stage.water) legacy->meshes.waterMeshes[i].loadComputedTerrain(std::move(*built.water),*stage.water,true);
                } else {
                    legacy->meshes.planetMeshes[i].loadTerrain(std::move(built.geometry));
                    if(built.water) legacy->meshes.waterMeshes[i].loadTerrain(std::move(*built.water));
                }
                tracking.triangles[i]=legacy->meshes.planetMeshes[i].indexCount/3;
            }
        }
        auto grassEye=terrainEye;std::optional<glm::dvec3> plannedEye;
        if(third) {
            if(resident) {
                auto planningScene=resident->scene();
                grassEye=previewReloadEye(planningScene,previewMeshes.planetMeshes,tracking.eyes,nextSource.replayDocument,nextOptions,time);
            } else grassEye=previewReloadEye(legacy->scene,legacy->meshes.planetMeshes,tracking.eyes,nextSource.replayDocument,nextOptions,time);
            plannedEye=grassEye;
        }
        for(std::size_t i=0;i<count;++i) {
            const auto& planet=stagedScene.scenario.planets[i];
            const auto anchor=i==selected && savedGrass ? *savedGrass : stagedScene.bodies[i+1].toLocalPoint(grassEye)/planet.radius;
            if(resident) {
                if(!resident->submit(std::move(*builds[i]),identities[i],anchor,*terrainCompute)) throw std::logic_error("Replacement terrain submission is busy");
                resident->waitForCapture();tracking.triangles[i]=resident->land(i).indexCount/3;
            } else legacy->grass.prepare(i,legacy->meshes.planetMeshes[i],planet,stagedScene.scenario.metersPerWorldUnit(),anchor);
        }
        const auto commit=prepareReloadCommit(stagedScene,third,time,plannedEye);
        if(resident) {
            if(!resident->publish(epoch)) throw std::logic_error("Replacement scene became obsolete");
            retiredResidentScene=std::move(resident);
        } else {
            legacy->retirementFence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
            if(!legacy->retirementFence) throw std::runtime_error("Scene retirement fence allocation failed");
            using std::swap;swap(scene,legacy->scene);source.document.swap(legacy->document);
            meshes.planetMeshes.swap(legacy->meshes.planetMeshes);meshes.waterMeshes.swap(legacy->meshes.waterMeshes);
            grass.procedural.swapState(legacy->grass.procedural);terrainSceneEpoch=epoch;
            retiredLegacyScene=std::move(legacy);
        }
        finishSceneReload(tracking,nextOptions,nextSource.replayDocument,commit);
    } catch(...) {++sceneReloadFailures;throw;}
}
nlohmann::json Renderer::Impl::sceneReloadState() const {
    nlohmann::json j={{"published",sceneReloads},{"failed",sceneReloadFailures},{"epoch",terrainSceneEpoch},
        {"retiring",bool(retiredResidentScene)||bool(retiredLegacyScene)},
        {"external_bytes",terrainPublication ? terrainPublication->externalBytes() : 0},
        {"pending",bool(pendingResidentReload)},{"superseded",sceneReloadSuperseded},{"progress_frames",sceneReloadFrames}};
    if(reloadCharacterEye) j["character_plan_eye"]={reloadCharacterEye->x,reloadCharacterEye->y,reloadCharacterEye->z};
    return j;
}
}
