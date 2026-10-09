#include "rendering/runtime/RendererState.h"
#include "rendering/foliage/GrassPlacement.h"
#include <algorithm>
#include <iostream>

namespace rendering {
PublicationProfiler::Generation Renderer::Impl::publicationGeneration(std::size_t i) const {
    const auto& land=meshes.planetMeshes[i];const auto& water=meshes.waterMeshes[i];
    const auto& l=land.terrainStats.generation;const auto& w=water.terrainStats.generation;
    PublicationProfiler::Generation result{l.field,l.topology,w.field,w.topology,land.revision,water.revision};
    if(const auto eye=grass.procedural.planningEye(i)) result.grassEye={eye->x,eye->y,eye->z};
    return result;
}
void Renderer::Impl::recordRenderedPublications(bool character) {
    auto& trace=profiler.publications();if(!trace.enabled()) return;
    trace.discardOlderEpochs(terrainSceneEpoch);
    for(std::size_t i=0;i<meshReady.size();++i) {
        if(!meshReady[i]) continue;
        const auto& land=meshes.planetMeshes[i];const auto& water=meshes.waterMeshes[i];
        if(character && i==scene.scenario.surface_camera.planet_index && astronautGround.revision()!=land.revision) continue;
        if(terrainPublication) {
            if(i>=terrainConsumers.size()) continue;
            const auto& c=terrainConsumers[i];
            if(c.land!=land.terrainStats.generation || c.contacts!=c.land || c.grass!=c.land ||
               c.water!=water.terrainStats.generation || c.landRevision!=land.revision ||
               c.mainRevision!=land.revision || c.grassRevision!=land.revision ||
               (c.shadowRevision && c.shadowRevision!=land.revision) ||
               (c.reflectionRevision && c.reflectionRevision!=land.revision) ||
               (c.grassDrawRevision && c.grassDrawRevision!=land.revision) ||
               (scene.scenario.planets[i].water.enabled && c.waterDrawRevision!=water.revision)) continue;
        }
        trace.rendered(terrainSceneEpoch,i,installedTerrainSerial[i],publicationGeneration(i),character && i==scene.scenario.surface_camera.planet_index);
    }
    trace.sceneRendered(terrainSceneEpoch);
}

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
    observeMemory(memoryPhase);
    ++residentFrames;plannedCharacterEye.reset();
    const auto identityFor=[&](std::size_t i) {
        TerrainBuildIdentity k;k.epoch=terrainSceneEpoch;k.serial=terrainRequestSerial;k.bodyIndex=i;
        k.bodyName=scene.scenario.planets[i].name;k.field=scene.terrainSurfaces[i].field().fingerprint();k.topologyVersion=scene.terrainSurfaces[i].topologyVersion();
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
    if(sceneReloadPreparing()) return; // Lease freezes live generations, not movement/draws.
    if(residentStage) {
        const auto k=*residentStage;
        try {
            // A grass-only stage deliberately keeps the installed serial.
            const bool compatible=k.bodyIndex<scene.scenario.planets.size() &&
                terrainBuildMatches(k,identityFor(k.bodyIndex),installedTerrainSerial[k.bodyIndex]-(residentStageGrassOnly ? 1 : 0));
            if(!compatible) {
                profiler.publications().finish(residentStageAttempt,PublicationProfiler::Outcome::Obsolete);
                terrainPublication->cancel();++terrainRejectedBuilds;residentStage.reset();
            }
            else if(terrainPublication->poll()) {
                profiler.publications().phase(residentStageAttempt,PublicationProfiler::Phase::GpuReady);
                if(terrainPublication->publish(identityFor(k.bodyIndex),meshes.planetMeshes[k.bodyIndex],meshes.waterMeshes[k.bodyIndex]))
                {
                    recordResidentPublication(k.bodyIndex,residentStageZones,!residentStageGrassOnly);
                    profiler.publications().prepared(residentStageAttempt,publicationGeneration(k.bodyIndex));
                } else {++terrainRejectedBuilds;profiler.publications().finish(residentStageAttempt,PublicationProfiler::Outcome::Obsolete);}
                residentStage.reset();glFlush();
            }
        } catch(const std::exception& error) {
            profiler.publications().finish(residentStageAttempt,PublicationProfiler::Outcome::PreparationFailed);
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
        if(!matches(*ready)) {
            auto completed=terrainJobs.poll();
            profiler.publications().finish(completed->traceAttempt,PublicationProfiler::Outcome::Obsolete);++terrainRejectedBuilds;
        }
        else if(terrainPublication->canSubmit(ready->bodyIndex)) {
            const auto k=*ready;
            auto completed=terrainJobs.poll();
            const auto attempt=completed->traceAttempt;
            try {
                auto built=completed->take();auto zones=built.geometry.faceZones;
                const auto world=planningEye(&built,k.bodyIndex);
                const auto anchor=scene.bodies[k.bodyIndex+1].toLocalPoint(world)/scene.scenario.planets[k.bodyIndex].radius;
                profiler.terrainBuild(built.milliseconds);
                profiler.publications().phase(attempt,PublicationProfiler::Phase::GpuSubmit);
                GpuWorkProfiler::Attempt binding(attempt);
                if(!terrainPublication->submit(std::move(built),k,scene.scenario.planets[k.bodyIndex],
                    scene.scenario.metersPerWorldUnit(),anchor,meshes.planetMeshes[k.bodyIndex],meshes.waterMeshes[k.bodyIndex],*terrainCompute))
                    throw std::logic_error("Resident preparation slot became busy");
                residentStage=k;residentStageAttempt=attempt;residentStageZones.swap(zones);residentStageGrassOnly=false;glFlush();
            } catch(const std::exception& error) {
                profiler.publications().finish(attempt,PublicationProfiler::Outcome::PreparationFailed);
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
    if(candidate) {
        candidate->identity.serial=++terrainRequestSerial;
        candidate->traceAttempt=profiler.publications().begin(PublicationProfiler::Key::from(candidate->identity));
        terrainJobs.submit(std::move(*candidate));
    }
    if(residentStage || !residentSceneReady()) return;
    const auto world=planningEye(nullptr,0);
    for(std::size_t i=0;i<scene.scenario.planets.size();++i) {
        const auto& planet=scene.scenario.planets[i];
        // Grass can reuse the installed terrain while its replacement is still
        // being planned. Waiting for that CPU job lets movement exhaust the
        // grass placement headroom before a new patch can be submitted.
        if(!planet.foliage.enabled || !terrainPublication->canSubmit(i)) continue;
        const auto current=identityFor(i);
        if(current.localMask!=lastLocalMask[i]) continue;
        if(terrainFailures[i] && residentFrames<residentRetryAfter) continue;
        const auto anchor=scene.bodies[i+1].toLocalPoint(world)/planet.radius;
        const auto previous=grass.procedural.planningEye(i);
        // Spend only half of the existing placement margin before refreshing;
        // leave the other half for nonblocking GPU preparation/publication.
        // Capture/replay keeps its original refresh schedule and margin.
        if(!grass.procedural.policyChanged(i) && previous && glm::length(anchor-*previous)*planet.radius*scene.scenario.metersPerWorldUnit()<0.5*grassRebuildDistance(planet.foliage)) continue;
        const auto k=terrainPublication->installed(i).identity;
        auto key=PublicationProfiler::Key::from(k);key.eye={anchor.x,anchor.y,anchor.z};
        const auto attempt=profiler.publications().begin(key,PublicationProfiler::Kind::Grass);
        try {
            profiler.publications().phase(attempt,PublicationProfiler::Phase::GpuSubmit);
            GpuWorkProfiler::Attempt binding(attempt);
            if(terrainPublication->submitGrass(i,planet,scene.scenario.metersPerWorldUnit(),anchor,
                meshes.planetMeshes[i],meshes.waterMeshes[i])) {
                residentStage=k;residentStageAttempt=attempt;residentStageGrassOnly=true;glFlush();break;
            }
            profiler.publications().finish(attempt,PublicationProfiler::Outcome::Rejected);
        } catch(const std::exception& error) {
            profiler.publications().finish(attempt,PublicationProfiler::Outcome::PreparationFailed);
            terrainPublication->cancel();residentStage.reset();fail(k,error);
        }
    }
}
}
