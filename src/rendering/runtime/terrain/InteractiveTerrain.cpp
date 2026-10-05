#include "rendering/runtime/RendererState.h"
#include "rendering/foliage/GrassPlacement.h"
#include <algorithm>
#include <iostream>

namespace rendering {
bool Renderer::Impl::residentSceneReady() const {
    return std::all_of(meshReady.begin(),meshReady.end(),[](bool ready){return ready;});
}
void Renderer::Impl::recordResidentPublication(std::size_t i,std::vector<int>& zones,bool terrainChanged) {
    const auto& receipt=terrainPublication->installed(i);
    terrainFailures[i].reset();
    if(terrainChanged) {
        lastFaceZones[i].swap(zones);meshZoneFaces[i]=receipt.zoneFaces;meshTriangles[i]=receipt.triangles;
        meshSteepRefinedFaces[i]=receipt.steepRefinedFaces;lastTerrainEyes[i]=receipt.identity.eye;
        lastLocalMask[i]=receipt.identity.localMask;meshReady[i]=true;
        installedTerrainSerial[i]=receipt.identity.serial;
        terrainShadows.invalidate(i);profiler.meshUpload();
        if(i==scene.scenario.surface_camera.planet_index)
            astronautGround.bind(meshes.planetMeshes[i].contacts,receipt.landRevision);
    }
    frameReuse.invalidate();
    profiler.foliagePreparation(1,0,0,0,grass.procedural.stats(i).metadataInputBytes+
        grass.procedural.stats(i).allocationInputBytes);
}
void Renderer::Impl::prepareResidentFrame(const glm::dvec3& eye,std::optional<double> characterElapsed) {
    CpuTrace::Scope scope("terrain.interactive_frame");
    ++residentFrames;plannedCharacterEye.reset();
    const auto identityFor=[&](std::size_t i) {
        TerrainBuildIdentity k;k.epoch=terrainSceneEpoch;k.serial=terrainRequestSerial;k.bodyIndex=i;
        k.bodyName=scene.scenario.planets[i].name;k.field=scene.terrainSurfaces[i].field().fingerprint();
        k.backend=TerrainBackend::Compute;k.resident=true;k.eye=scene.bodies[i+1].toLocalPoint(eye);
        const auto distance=glm::length(k.eye);
        if(!std::isfinite(distance) || distance<=0) throw std::invalid_argument("Camera cannot be at a planet center");
        k.localMask=distance<3*scene.scenario.planets[i].radius ? 1 : 0;return k;
    };
    const auto matches=[&](const TerrainBuildIdentity& k) {
        return k.bodyIndex<scene.scenario.planets.size() &&
            terrainBuildMatches(k,identityFor(k.bodyIndex),installedTerrainSerial[k.bodyIndex]);
    };
    const auto fail=[&](const TerrainBuildIdentity& k,const std::exception& error) {
        if(k.bodyIndex<terrainFailures.size()) terrainFailures[k.bodyIndex]=k;
        ++residentFrameFailures;residentRetryAfter=residentFrames+60;
        std::cerr << "Terrain preparation failed; previous generation retained: " << error.what() << '\n';
    };
    try {
        retireSceneReload(false);terrainPublication->pollRetired();residentRetirementFailed=false;
    } catch(const std::exception& error) {
        if(!residentRetirementFailed) {
            ++residentFrameFailures;std::cerr << "Terrain retirement pending: " << error.what() << '\n';
        }
        residentRetirementFailed=true;return; // Keep old resources and admission charged.
    }
    if(residentStage) {
        const auto k=*residentStage;
        try {
            // A grass-only stage deliberately keeps the installed serial.
            const bool compatible=k.bodyIndex<scene.scenario.planets.size() &&
                terrainBuildMatches(k,identityFor(k.bodyIndex),installedTerrainSerial[k.bodyIndex]-(residentStageGrassOnly ? 1 : 0));
            if(!compatible) {terrainPublication->cancel();++terrainRejectedBuilds;residentStage.reset();}
            else if(terrainPublication->poll()) {
                if(terrainPublication->publish(identityFor(k.bodyIndex),meshes.planetMeshes[k.bodyIndex],meshes.waterMeshes[k.bodyIndex]))
                    recordResidentPublication(k.bodyIndex,residentStageZones,!residentStageGrassOnly);
                else ++terrainRejectedBuilds;
                residentStage.reset();glFlush();
            }
        } catch(const std::exception& error) {
            terrainPublication->cancel();residentStage.reset();fail(k,error);
        }
    }
    const auto planningEye=[&](const TerrainCpuBuild* build,std::size_t body) {
        if(!characterElapsed || !residentSceneReady()) return eye;
        auto contacts=characterTerrainContacts();
        if(build) contacts[body].bind(build->contacts,meshes.planetMeshes[body].revision+1);
        return previewAstronautEye(*characterElapsed,std::move(contacts));
    };
    if(const auto ready=terrainJobs.readyIdentity()) {
        if(!matches(*ready)) {terrainJobs.poll();++terrainRejectedBuilds;}
        else if(terrainPublication->canSubmit(ready->bodyIndex)) {
            const auto k=*ready;
            auto completed=terrainJobs.poll();
            try {
                auto built=completed->take();auto zones=built.geometry.faceZones;
                const auto world=planningEye(&built,k.bodyIndex);
                const auto anchor=scene.bodies[k.bodyIndex+1].toLocalPoint(world)/scene.scenario.planets[k.bodyIndex].radius;
                profiler.terrainBuild(built.milliseconds);
                if(!terrainPublication->submit(std::move(built),k,scene.scenario.planets[k.bodyIndex],
                    scene.scenario.metersPerWorldUnit(),anchor,meshes.planetMeshes[k.bodyIndex],meshes.waterMeshes[k.bodyIndex],*terrainCompute))
                    throw std::logic_error("Resident preparation slot became busy");
                residentStage=k;residentStageZones.swap(zones);residentStageGrassOnly=false;glFlush();
            } catch(const std::exception& error) {
                terrainPublication->cancel();residentStage.reset();fail(k,error);
            }
        }
        // Busy GPU/retirement leaves the result inside the worker's one ready
        // slot, so it cannot begin another queued build or accumulate outputs.
    }
    const auto movedFrom=[&](std::size_t i,const glm::dvec3& current,const glm::dvec3& previous) {
        return scene.scenario.planets[i].radius*scene.scenario.metersPerWorldUnit()*std::acos(std::clamp(
            glm::dot(glm::normalize(current),glm::normalize(previous)),-1.0,1.0));
    };
    std::optional<TerrainBuildRequest> candidate;double priority=std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<scene.scenario.planets.size();++i) {
        auto k=identityFor(i);const auto& planet=scene.scenario.planets[i];
        if(meshReady[i] && k.localMask==lastLocalMask[i] &&
            (!k.localMask || movedFrom(i,k.eye,lastTerrainEyes[i])<10)) continue;
        const auto covered=[&](const TerrainBuildIdentity& pending) {
            return pending.epoch==k.epoch && pending.field==k.field && pending.localMask==k.localMask &&
                movedFrom(i,k.eye,pending.eye)<10;
        };
        if(residentStage && residentStage->bodyIndex==i && !residentStageGrassOnly && covered(*residentStage)) continue;
        if(const auto pending=terrainJobs.pendingFor(k.epoch,i);pending && covered(*pending)) continue;
        if(terrainFailures[i] && covered(*terrainFailures[i]) && residentFrames<residentRetryAfter) continue;
        const double distance=glm::length(k.eye)/planet.radius;
        if(distance<priority) {
            priority=distance;candidate=TerrainBuildRequest{k,scene.terrainSurfaces[i],planet,lastFaceZones[i],scene.scenario.metersPerWorldUnit()};
        }
    }
    if(candidate) {candidate->identity.serial=++terrainRequestSerial;terrainJobs.submit(std::move(*candidate));}
    if(residentStage || !residentSceneReady()) return;
    const auto world=planningEye(nullptr,0);
    for(std::size_t i=0;i<scene.scenario.planets.size();++i) {
        const auto& planet=scene.scenario.planets[i];
        if(!planet.foliage.enabled || !terrainPublication->canSubmit(i) || terrainJobs.pendingFor(terrainSceneEpoch,i)) continue;
        const auto current=identityFor(i);
        if(current.localMask!=lastLocalMask[i]) continue;
        if(terrainFailures[i] && residentFrames<residentRetryAfter) continue;
        const auto anchor=scene.bodies[i+1].toLocalPoint(world)/planet.radius;
        const auto previous=grass.procedural.planningEye(i);
        if(previous && glm::length(anchor-*previous)*planet.radius*scene.scenario.metersPerWorldUnit()<grassRebuildDistance(planet.foliage)) continue;
        const auto k=terrainPublication->installed(i).identity;
        try {
            if(terrainPublication->submitGrass(i,planet,scene.scenario.metersPerWorldUnit(),anchor,
                meshes.planetMeshes[i],meshes.waterMeshes[i])) {
                residentStage=k;residentStageGrassOnly=true;glFlush();break;
            }
        } catch(const std::exception& error) {
            terrainPublication->cancel();residentStage.reset();fail(k,error);
        }
    }
}
}
