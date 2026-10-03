#include "rendering/runtime/RendererState.h"

namespace rendering {
void Renderer::Impl::installLandMesh(std::size_t index, TerrainGeometry geometry,
    const glm::dvec3& localEye, int localMask,TerrainComputeBuffers* computed) {
    CpuTrace::Scope scope("Renderer::installLandMesh");
    meshZoneFaces[index] = geometry.zoneFaces;
    meshTriangles[index] = geometry.triangleCount();
    meshSteepRefinedFaces[index] = geometry.steepRefinedFaces;
    lastFaceZones[index] = geometry.faceZones;
    if(computed) meshes.planetMeshes[index].loadComputedTerrain(std::move(geometry),*computed);
    else meshes.planetMeshes[index].loadTerrain(std::move(geometry));
    profiler.meshUpload();
    meshReady[index] = true;
    lastLocalMask[index] = localMask;
    lastTerrainEyes[index] = localEye;
}

void Renderer::Impl::preparePlanetMeshes(const glm::dvec3& eye, bool asyncWalking) {
    CpuTrace::Scope scope("Renderer::preparePlanetMeshes");
    for (std::size_t i = 0; i < scene.scenario.planets.size(); ++i) {
        const auto& planet = scene.scenario.planets[i];
        glm::dvec3 localEye = scene.bodies[i + 1].toLocalPoint(eye);
        if (options.renderTestMode && options.thirdPersonRenderMode && !meshReady[i] && !options.replayPath.empty()) {
            const auto replay=config::Config::load(options.replayPath).data();
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
        if (pendingTerrain[i].geometry.valid()) {
            if (pendingTerrain[i].geometry.wait_for(std::chrono::seconds(0)) !=
                std::future_status::ready) continue;
            auto built = pendingTerrain[i].geometry.get();
            profiler.terrainBuild(built.milliseconds);
            if (built.water) {
                meshes.waterMeshes[i].loadTerrain(std::move(*built.water));
            }
            installLandMesh(i, std::move(built.geometry),
                            pendingTerrain[i].eyeLocal,
                            pendingTerrain[i].localMask);
        }
        const double movedMeters = meshReady[i] ? planet.radius *
            scene.scenario.metersPerWorldUnit() * std::acos(std::clamp(
                glm::dot(radial, glm::normalize(lastTerrainEyes[i])), -1.0, 1.0)) : 0.0;
        if (meshReady[i] && localMask == lastLocalMask[i] &&
            (localMask == 0 || movedMeters < 10.0)) continue;
        // Land and the nearby ocean shell rebuild together off-thread.
        auto buildMeshes = [surface=scene.terrainSurfaces[i], planet,
                            zones=lastFaceZones[i], localEye,
                            compute=bool(terrainCompute),
                            meters=scene.scenario.metersPerWorldUnit()]() {
            CpuTrace::Scope scope("terrain.build_land_and_water");
            const auto start=std::chrono::steady_clock::now();
            std::optional<TerrainTopology> topology,waterTopology;
            std::optional<PlanetField> waterField;
            if(compute) topology=surface.buildTopologyForEye(localEye,glm::dvec3(0.0),
                zones.empty()?nullptr:&zones,20.0);
            auto land=compute?surface.evaluateTopology(*topology):surface.buildGeometryForEye(localEye, glm::dvec3(0.0),
                zones.empty() ? nullptr : &zones, 20.0);
            std::optional<rendering::TerrainGeometry> water;
            if (planet.water.enabled) {
                auto lod=planet.terrain_lod;
                lod.base_edge_segments=1;
                lod.medium_edge_segments=3;
                lod.max_edge_segments=8;
                lod.steep_edge_segments=8;
                lod.sink_depth_m=0.0; // The sea remains at its configured physical level.
                lod.near_surface_distance_m=lod.shoreline_distance_m;
                lod.mid_surface_distance_m=2*lod.shoreline_distance_m;
                lod.max_triangle_budget=std::min(60000,lod.max_triangle_budget);
                const double seaRadius=planet.radius+planet.water.level_m/meters;
                const rendering::TerrainSurface sea({},lod,seaRadius,meters,{},0.0);
                if(compute) {
                    waterTopology=sea.buildTopologyForEye(localEye,glm::dvec3(0.0));
                    waterField=sea.field();water=sea.evaluateTopology(*waterTopology);
                } else water=sea.buildGeometryForEye(localEye,glm::dvec3(0.0));
            }
            return TimedTerrainBuild{std::move(land),std::move(water),
                std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),
                std::move(topology),std::move(waterTopology),std::move(waterField)};
        };
        if (asyncWalking && meshReady[i] && localMask == lastLocalMask[i]) {
            pendingTerrain[i].eyeLocal = localEye;
            pendingTerrain[i].localMask = localMask;
            pendingTerrain[i].geometry = std::async(std::launch::async,
                [trace=&cpuTrace, build=std::move(buildMeshes)]() {
                    CpuTrace::Thread thread(trace, "terrain worker");
                    return build();
                });
            continue;
        }
        auto built=buildMeshes();
        if (options.renderTestMode && i==scene.scenario.surface_camera.planet_index)
            captureTerrainEye=localEye;
        profiler.terrainBuild(built.milliseconds);
        std::unique_ptr<TerrainComputeBuffers> computed,computedWater;
        if(terrainCompute) {
            computed=terrainCompute->generate(scene.terrainSurfaces[i].field(),*built.topology);
            if(built.water) computedWater=terrainCompute->generate(*built.waterField,*built.waterTopology);
            computed->waitForCapture();if(computedWater) computedWater->waitForCapture();
            // Both outputs are complete before either mesh becomes visible.
        }
        if (built.water) {
            if(computedWater) meshes.waterMeshes[i].loadComputedTerrain(std::move(*built.water),*computedWater);
            else meshes.waterMeshes[i].loadTerrain(std::move(*built.water));
        }
        installLandMesh(i, std::move(built.geometry), localEye, localMask,computed.get());
    }
}

std::vector<std::uint64_t> Renderer::Impl::geometryRevisions() const {
    std::vector<std::uint64_t> result;
    for (const auto& mesh : meshes.planetMeshes) result.push_back(mesh.revision);
    return result;
}
}
