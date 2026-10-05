#include "rendering/runtime/RendererState.h"
#include "rendering/runtime/terrain/reload/interactive/PendingReload.h"
#include "rendering/runtime/terrain/reload/ReloadCommit.h"
#include "rendering/runtime/terrain/reload/ReplayEffects.h"
#include "rendering/quality/OfflineQuality.h"
#include "config/SceneReplay.h"
#include <iostream>
namespace rendering {
namespace {
glm::dvec3 readEye(const nlohmann::json& j) {
    if(!j.is_array() || j.size()!=3) throw std::invalid_argument("Reload planning eye needs three values");
    const glm::dvec3 eye{j.at(0).get<double>(),j.at(1).get<double>(),j.at(2).get<double>()};
    if(!std::isfinite(glm::length(eye)) || glm::length(eye)<=0) throw std::invalid_argument("Invalid reload planning eye");
    return eye;
}
}
bool Renderer::Impl::sceneReloadPreparing() const {
    return pendingResidentReload && bool(pendingResidentReload->transaction);
}
void Renderer::Impl::requestResidentReload() {
    CpuTrace::Scope scope("scene.reload_request");
    if(pendingResidentReload) {
        terrainJobs.abortReplacement(pendingResidentReload->epoch);
        pendingResidentReload.reset();++sceneReloadSuperseded;
    }
    try {
        lastReloadAttempt=std::max(lastReloadAttempt,terrainSceneEpoch);
        if(lastReloadAttempt==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Scene epoch exhausted");
        const auto epoch=++lastReloadAttempt;
        auto nextOptions=options;auto nextSource=app::SceneSource::forResidentReload(nextOptions);
        if(nextOptions.terrainBackend!=options.terrainBackend || nextOptions.terrainGrassPlanner!=options.terrainGrassPlanner ||
           nextOptions.renderTestWidth!=options.renderTestWidth || nextOptions.renderTestHeight!=options.renderTestHeight ||
           nextOptions.offlineQuality!=options.offlineQuality || nextOptions.atmosphereFullResolution!=options.atmosphereFullResolution ||
           nextOptions.lensFlare!=options.lensFlare)
            throw std::invalid_argument("Reload requires unchanged renderer pipeline and capture dimensions");
        app::PreparedScene prepared(offlineScenario(config::ScenarioConfig{config::Config{nlohmann::json(nextSource.document)}},nextOptions));
        const double time=config::replayStartTime(prepared.scenario,nextOptions.commandLineTime);prepared.updateSimulation(time);
        const bool third=cameraInput.mode()==CameraMode::ThirdPerson && prepared.surfaceCamera;
        const auto terrainEye=(cameraInput.mode()==CameraMode::Surface || third) && prepared.surfaceCamera ? prepared.surfaceCamera->position() :
            cameraInput.mode()==CameraMode::PlanetOrbit && prepared.planetOrbitCamera ? glm::dvec3(prepared.planetOrbitCamera->position) : glm::dvec3(prepared.sunCamera.position);
        if(third && nextSource.replayDocument.contains("astronaut_pose"))
            characterReplay::validateEffects(nextSource.replayDocument.at("astronaut_pose"));
        for(const auto& planet:prepared.scenario.planets)
            if(planet.foliage.enabled && !planet.foliage.compute_placement)
                throw std::invalid_argument("Resident reload requires GPU foliage placement");
        auto pending=std::make_unique<PendingSceneReload>(std::move(nextOptions),std::move(nextSource.document),
            std::move(nextSource.replayDocument),std::move(prepared),epoch,time);
        pending->third=third;pending->terrainEye=terrainEye;
        const auto& s=*pending->prepared;const auto count=s.scenario.planets.size();
        const auto* pose=third && pending->replay.contains("astronaut_pose") ? &pending->replay.at("astronaut_pose") : nullptr;
        if(pose && pose->contains("grass_plan_eye")) pending->savedGrass=readEye(pose->at("grass_plan_eye"));
        for(std::size_t i=0;i<count;++i) {
            auto eye=s.bodies[i+1].toLocalPoint(terrainEye);
            if(pose) {
                if(pose->contains("terrain_plan_eyes_world_units")) {
                    const auto& eyes=pose->at("terrain_plan_eyes_world_units");
                    if(!eyes.is_array() || eyes.size()!=count) throw std::invalid_argument("Reload replay needs one terrain anchor per body");
                    eye=readEye(eyes.at(i));
                } else if(i==s.scenario.surface_camera.planet_index && pose->contains("terrain_plan_eye_world_units"))
                    eye=readEye(pose->at("terrain_plan_eye_world_units"));
                if(pose->contains("terrain_face_zones")) {
                    const auto& zones=pose->at("terrain_face_zones");
                    if(!zones.is_array() || zones.size()!=count) throw std::invalid_argument("Reload replay needs terrain zones for every body");
                    pending->tracking.zones[i]=zones.at(i).get<std::vector<int>>();
                }
            }
            pending->tracking.eyes[i]=eye;
        }
        pendingResidentReload=std::move(pending);
    } catch(...) {++sceneReloadFailures;throw;}
}
bool Renderer::Impl::pollResidentReload() {
    if(!pendingResidentReload) return false;
    CpuTrace::Scope scope("scene.reload_progress");++sceneReloadFrames;
    // A failed old-scene poll retains both retirement and the latest request.
    try {retireSceneReload(false);terrainPublication->pollRetired();}
    catch(const std::exception& error) {
        if(!residentRetirementFailed) std::cerr << "Scene retirement pending: " << error.what() << '\n';
        residentRetirementFailed=true;return false;
    }
    try {
        auto& p=*pendingResidentReload;
        if(!p.transaction) {
            if(retiredResidentScene || retiredLegacyScene || !residentSceneReady()) return false;
            terrainPublication->cancel();residentStage.reset();residentStageZones.clear();
            terrainJobs.beginReplacement(p.epoch);
            p.transaction=std::make_unique<SceneTerrainReplacement>(p.document,std::move(*p.prepared),
                SceneTerrainDestination{scene,source.document,meshes.planetMeshes,meshes.waterMeshes,
                    grass.procedural,*terrainPublication,terrainSceneEpoch},p.epoch,p.time);
            p.prepared.reset();
        }
        auto& transaction=*p.transaction;transaction.poll();
        if(p.gpuBody && !transaction.preparing()) {
            const auto i=*p.gpuBody;p.tracking.triangles[i]=transaction.land(i).indexCount/3;
            p.gpuBody.reset();++p.nextBody;
        }
        if(p.grassBody && !transaction.preparing()) {p.grassBody.reset();++p.nextGrass;}
        if(p.cpuBody) {
            if(const auto ready=terrainJobs.readyIdentity()) {
                auto result=terrainJobs.poll();
                if(*ready!=*p.cpuIdentity) {++terrainRejectedBuilds;throw std::logic_error("Replacement worker identity changed");}
                auto built=result->take();const auto i=*p.cpuBody;
                p.tracking.record(i,built,*ready);profiler.terrainBuild(built.milliseconds);
                const auto& s=transaction.scene();
                const auto anchor=s.bodies[i+1].toLocalPoint(p.terrainEye)/s.scenario.planets[i].radius;
                if(!transaction.submit(std::move(built),*ready,anchor,*terrainCompute))
                    throw std::logic_error("Replacement GPU preparation slot is busy");
                p.cpuBody.reset();p.cpuIdentity.reset();p.gpuBody=i;glFlush();
            }
            return false;
        }
        const auto count=p.tracking.ready.size();
        if(p.nextBody<count) {
            if(p.gpuBody) return false;
            const auto i=p.nextBody;
            auto request=transaction.requestLocal(i,p.tracking.eyes[i],++terrainRequestSerial,std::move(p.tracking.zones[i]));
            p.cpuIdentity=request.identity;
            if(!terrainJobs.submit(std::move(request))) throw std::logic_error("Replacement CPU preparation slot is busy");
            p.cpuBody=i;return false;
        }
        if(!p.planned) {
            if(p.third) {
                auto prepared=transaction.scene();
                for(std::size_t i=0;i<count;++i) {
                    p.previewLand[i].contacts=transaction.land(i).contacts;p.previewLand[i].revision=transaction.land(i).revision;
                }
                p.characterEye=previewReloadEye(prepared,p.previewLand,p.tracking.eyes,p.replay,p.options,p.time);
            }
            p.planned=true;
        }
        if(p.nextGrass<count) {
            if(p.grassBody) return false;
            const auto i=p.nextGrass;const auto& s=transaction.scene();
            const auto anchor=i==s.scenario.surface_camera.planet_index && p.savedGrass ? *p.savedGrass :
                s.bodies[i+1].toLocalPoint(p.characterEye.value_or(p.terrainEye))/s.scenario.planets[i].radius;
            if(!transaction.replanGrass(i,anchor)) return false;
            if(transaction.preparing()) {p.grassBody=i;glFlush();}
            else ++p.nextGrass;
            return false;
        }
        const auto commit=prepareReloadCommit(transaction.scene(),p.third,p.time,p.characterEye);
        if(!transaction.publish(p.epoch)) throw std::logic_error("Replacement scene became obsolete");
        retiredResidentScene=std::move(p.transaction);
        finishSceneReload(p.tracking,p.options,p.replay,commit);
        pendingResidentReload.reset();return true;
    } catch(const std::exception& error) {
        const auto epoch=pendingResidentReload->epoch;terrainJobs.abortReplacement(epoch);
        pendingResidentReload.reset();++sceneReloadFailures;
        std::cerr << "Config reload failed; current scene retained: " << error.what() << '\n';return false;
    }
}
}
