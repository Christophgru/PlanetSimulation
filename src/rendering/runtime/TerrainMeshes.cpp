#include "rendering/runtime/RendererState.h"
#include "rendering/geometry/compute/TerrainGpuPreparation.h"
#include <iostream>

namespace rendering {
void Renderer::Impl::installLandMesh(std::size_t index, TerrainGeometry geometry,
    const glm::dvec3& localEye, int localMask,TerrainComputeBuffers* computed) {
    CpuTrace::Scope scope("Renderer::installLandMesh");
    meshZoneFaces[index] = geometry.zoneFaces;
    meshTriangles[index] = computed ? computed->stats.gpuCorners/3 : geometry.triangleCount();
    meshSteepRefinedFaces[index] = geometry.steepRefinedFaces;
    lastFaceZones[index] = geometry.faceZones;
    if(computed) meshes.planetMeshes[index].loadComputedTerrain(std::move(geometry),*computed,options.terrainGrassPlanner=="cpu");
    else meshes.planetMeshes[index].loadTerrain(std::move(geometry));
    profiler.meshUpload();
    meshReady[index] = true;
    lastLocalMask[index] = localMask;
    lastTerrainEyes[index] = localEye;
}

void Renderer::Impl::installTerrainBuild(TerrainCpuBuild built,const TerrainBuildIdentity& identity) {
    if(identity.resident) throw std::logic_error("Resident terrain requires complete publication");
    const auto i=identity.bodyIndex;
    profiler.terrainBuild(built.milliseconds);
    if(identity.backend==TerrainBackend::Cpu) {
        // CPU generation can occupy the first native frame. Consume the
        // worker's current cache before admission, not only at frame end.
        observeMemory(memoryPhase);
        const auto bytes=[](const TerrainGeometry& g) {return g.vertices.size()*sizeof(float)+g.indices.size()*sizeof(unsigned);};
        grass.procedural.admitCpuTerrain(bytes(built.geometry)+(built.water?bytes(*built.water):0));
    }
    std::unique_ptr<TerrainComputeBuffers> computed,computedWater;
    if(identity.backend==TerrainBackend::Compute) {
        const auto attempt=profiler.publications().terrain(identity.epoch,i,identity.serial);
        profiler.publications().phase(attempt,PublicationProfiler::Phase::GpuSubmit);
        TerrainGpuPreparation staged(std::move(built),identity,scene.scenario.planets[i],*terrainCompute);
        // Explicit capture adapter. Interactive compute remains gated until
        // complete frame-boundary publication/recovery is implemented in T3c3.
        staged.waitForCapture();
        profiler.publications().phase(attempt,PublicationProfiler::Phase::GpuReady);
        built=std::move(staged.cpu);computed=std::move(staged.land);computedWater=std::move(staged.water);
    }
    if(built.water) {
        if(computedWater) meshes.waterMeshes[i].loadComputedTerrain(std::move(*built.water),*computedWater,!identity.resident);
        else meshes.waterMeshes[i].loadTerrain(std::move(*built.water));
    }
    installLandMesh(i,std::move(built.geometry),identity.eye,identity.localMask,computed.get());
    installedTerrainSerial[i]=identity.serial;terrainFailures[i].reset();
}

void Renderer::Impl::preparePlanetMeshes(const glm::dvec3& eye, bool asyncWalking,std::optional<double> characterElapsed) {
    CpuTrace::Scope scope("Renderer::preparePlanetMeshes");
    profiler.publications().captureMode(options.renderTestMode);
    if(terrainPublication && !options.renderTestMode) {
        prepareResidentFrame(eye,characterElapsed);return;
    }
    retireSceneReload(options.renderTestMode);
    const auto identityFor=[&](std::size_t i,const glm::dvec3& localEye,int localMask) {
        TerrainBuildIdentity k;k.epoch=terrainSceneEpoch;k.serial=terrainRequestSerial;
        k.bodyIndex=i;k.bodyName=scene.scenario.planets[i].name;k.field=scene.terrainSurfaces[i].field().fingerprint();k.topologyVersion=scene.terrainSurfaces[i].topologyVersion();
        k.backend=terrainCompute ? TerrainBackend::Compute : TerrainBackend::Cpu;
        k.resident=bool(terrainCompute) && options.terrainGrassPlanner=="gpu-v1";
        k.eye=localEye;k.localMask=localMask;return k;
    };
    if(auto completed=terrainJobs.poll()) {
        const auto& k=completed->identity;
        if(k.bodyIndex<scene.scenario.planets.size()) {
            const auto currentEye=scene.bodies[k.bodyIndex+1].toLocalPoint(eye);
            const auto current=identityFor(k.bodyIndex,currentEye,
                glm::length(currentEye)<3*scene.scenario.planets[k.bodyIndex].radius ? 1 : 0);
            if(terrainBuildMatches(k,current,installedTerrainSerial[k.bodyIndex])) {
                try {
                    GpuWorkProfiler::Attempt binding(completed->traceAttempt);
                    installTerrainBuild(completed->take(),k);
                    profiler.publications().prepared(completed->traceAttempt,publicationGeneration(k.bodyIndex));
                }
                catch(const std::exception& error) {
                    profiler.publications().finish(completed->traceAttempt,PublicationProfiler::Outcome::PreparationFailed);
                    terrainFailures[k.bodyIndex]=k;
                    std::cerr << "Terrain build failed; previous generation retained: " << error.what() << '\n';
                }
            } else {++terrainRejectedBuilds;profiler.publications().finish(completed->traceAttempt,PublicationProfiler::Outcome::Obsolete);}
        } else {++terrainRejectedBuilds;profiler.publications().finish(completed->traceAttempt,PublicationProfiler::Outcome::Obsolete);}
    }
    std::optional<TerrainBuildRequest> candidate;
    std::vector<std::optional<TerrainCpuBuild>> residentBuilds(terrainPublication ? scene.scenario.planets.size() : 0);
    std::vector<std::optional<TerrainBuildIdentity>> residentIdentities(residentBuilds.size());
    double candidateDistance=std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < scene.scenario.planets.size(); ++i) {
        const auto& planet = scene.scenario.planets[i];
        glm::dvec3 localEye = scene.bodies[i + 1].toLocalPoint(eye);
        if (options.renderTestMode && options.thirdPersonRenderMode && !meshReady[i] && !options.replayPath.empty()) {
            const auto& replay=source.replayDocument;
            if (replay.contains("astronaut_pose")) {
                const auto& pose=replay.at("astronaut_pose");
                const nlohmann::json* saved=nullptr;
                if (pose.contains("terrain_plan_eyes_world_units")) {
                    const auto& eyes=pose.at("terrain_plan_eyes_world_units");
                    if (!eyes.is_array() || eyes.size()!=scene.scenario.planets.size())
                        throw std::invalid_argument("Astronaut replay requires one terrain anchor per planet");
                    saved=&eyes.at(i);
                } else if (i==scene.scenario.surface_camera.planet_index && pose.contains("terrain_plan_eye_world_units"))
                    saved=&pose.at("terrain_plan_eye_world_units");
                if (saved) {
                    if (!saved->is_array() || saved->size()!=3)
                        throw std::invalid_argument("Astronaut terrain replay requires a three-vector eye");
                    localEye={saved->at(0).get<double>(),saved->at(1).get<double>(),saved->at(2).get<double>()};
                    if (!std::isfinite(localEye.x)||!std::isfinite(localEye.y)||!std::isfinite(localEye.z))
                        throw std::invalid_argument("Astronaut terrain replay eye must be finite");
                }
                if (pose.contains("terrain_face_zones")) {
                    const auto& zones=pose.at("terrain_face_zones");
                    if (!zones.is_array() || zones.size()!=scene.scenario.planets.size())
                        throw std::invalid_argument("Astronaut replay requires terrain zones for each planet");
                    lastFaceZones[i]=zones.at(i).get<std::vector<int>>();
                }
            }
        }
        const glm::dvec3 offset = localEye;
        const double distance = glm::length(offset);
        if (!std::isfinite(distance) || distance <= 0.0)
            throw std::invalid_argument("Camera cannot be at a planet center");
        const glm::dvec3 radial = offset / distance;
        const int localMask = distance < 3.0 * planet.radius ? 1 : 0;
        const double movedMeters = meshReady[i] ? planet.radius *
            scene.scenario.metersPerWorldUnit() * std::acos(std::clamp(
                glm::dot(radial, glm::normalize(lastTerrainEyes[i])), -1.0, 1.0)) : 0.0;
        if (meshReady[i] && localMask == lastLocalMask[i] &&
            (localMask == 0 || movedMeters < 10.0)) continue;
        const auto movedFrom=[&](const glm::dvec3& anchor) {
            return planet.radius*scene.scenario.metersPerWorldUnit()*std::acos(std::clamp(
                glm::dot(radial,glm::normalize(anchor)),-1.0,1.0));
        };
        if(auto pending=terrainJobs.pendingFor(terrainSceneEpoch,i)) {
            if(pending->localMask==localMask && movedFrom(pending->eye)<10) continue;
        }
        if(terrainFailures[i] && terrainFailures[i]->localMask==localMask &&
            movedFrom(terrainFailures[i]->eye)<10) continue;
        auto identity=identityFor(i,localEye,localMask);
        identity.serial=++terrainRequestSerial;
        TerrainBuildRequest request{identity,scene.terrainSurfaces[i],planet,lastFaceZones[i],
            scene.scenario.metersPerWorldUnit()};
        if(asyncWalking && meshReady[i] && localMask==lastLocalMask[i]) {
            // Queue one closest-body request after examining every body. Moving
            // beyond a pending anchor coalesces a follow-up without starving it.
            const double priority=distance/planet.radius;
            if(priority<candidateDistance) {candidateDistance=priority;candidate=std::move(request);}
            continue;
        }
        request.traceAttempt=profiler.publications().begin(PublicationProfiler::Key::from(identity));
        const auto attempt=request.traceAttempt;
        TerrainCpuBuild built;
        try {
            if(terrainCompute) built=terrainJobs.executeForCapture(std::move(request)).take();
            else {
                profiler.publications().phase(attempt,PublicationProfiler::Phase::CpuStart);
                built=buildTerrainCpu(request);
                profiler.publications().phase(attempt,PublicationProfiler::Phase::CpuEnd);
            }
        } catch(...) {profiler.publications().finish(attempt,PublicationProfiler::Outcome::CpuFailed);throw;}
        if(options.renderTestMode && i==scene.scenario.surface_camera.planet_index) captureTerrainEye=localEye;
        if(identity.resident) {
            residentBuilds[i]=std::move(built);residentIdentities[i]=identity;
        } else {
            try {
                GpuWorkProfiler::Attempt binding(attempt);installTerrainBuild(std::move(built),identity);
                profiler.publications().prepared(attempt,publicationGeneration(i));
            } catch(...) {profiler.publications().finish(attempt,PublicationProfiler::Outcome::PreparationFailed);throw;}
        }
    }
    if(candidate) {
        candidate->traceAttempt=profiler.publications().begin(PublicationProfiler::Key::from(candidate->identity));
        terrainJobs.submit(std::move(*candidate));
    }
    if(terrainPublication) {
        try {publishResidentBuilds(std::move(residentBuilds),residentIdentities,eye,characterElapsed);}
        catch(...) {
            for(const auto& k:residentIdentities) if(k)
                profiler.publications().finish(profiler.publications().terrain(k->epoch,k->bodyIndex,k->serial),PublicationProfiler::Outcome::PreparationFailed);
            throw;
        }
    }
}

std::vector<std::uint64_t> Renderer::Impl::geometryRevisions() const {
    std::vector<std::uint64_t> result;
    for (const auto& mesh : meshes.planetMeshes) result.push_back(mesh.revision);
    return result;
}
}
