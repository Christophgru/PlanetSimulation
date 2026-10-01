#include "rendering/runtime/RendererState.h"

namespace rendering {
void Renderer::Impl::installLandMesh(std::size_t index, TerrainGeometry geometry,
    const glm::dvec3& radial, int localMask) {
    CpuTrace::Scope scope("Renderer::installLandMesh");
    meshZoneFaces[index] = geometry.zoneFaces;
    meshTriangles[index] = geometry.triangleCount();
    meshSteepRefinedFaces[index] = geometry.steepRefinedFaces;
    lastFaceZones[index] = geometry.faceZones;
    meshes.planetMeshes[index].loadTerrain(std::move(geometry));
    profiler.meshUpload();
    meshReady[index] = true;
    lastLocalMask[index] = localMask;
    lastEyeRadial[index] = radial;
}

void Renderer::Impl::preparePlanetMeshes(const glm::dvec3& eye, bool asyncWalking) {
    CpuTrace::Scope scope("Renderer::preparePlanetMeshes");
    for (std::size_t i = 0; i < scene.scenario.planets.size(); ++i) {
        const auto& planet = scene.scenario.planets[i];
        const glm::dvec3 localEye = scene.bodies[i + 1].toLocalPoint(eye);
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
                            pendingTerrain[i].eyeRadial,
                            pendingTerrain[i].localMask);
        }
        const double movedMeters = meshReady[i] ? planet.radius *
            scene.scenario.metersPerWorldUnit() * std::acos(std::clamp(
                glm::dot(radial, lastEyeRadial[i]), -1.0, 1.0)) : 0.0;
        if (meshReady[i] && localMask == lastLocalMask[i] &&
            (localMask == 0 || movedMeters < 10.0)) continue;
        // Land and the nearby ocean shell rebuild together off-thread.
        auto buildMeshes = [surface=scene.terrainSurfaces[i], planet,
                            zones=lastFaceZones[i], localEye,
                            meters=scene.scenario.metersPerWorldUnit()]() {
            CpuTrace::Scope scope("terrain.build_land_and_water");
            const auto start=std::chrono::steady_clock::now();
            auto land=surface.buildGeometryForEye(localEye, glm::dvec3(0.0),
                zones.empty() ? nullptr : &zones, 20.0);
            std::optional<rendering::TerrainGeometry> water;
            if (planet.water.enabled) {
                auto lod=planet.terrain_lod;
                lod.base_edge_segments=1;
                lod.medium_edge_segments=3;
                lod.max_edge_segments=8;
                lod.steep_edge_segments=8;
                lod.near_surface_distance_m=lod.shoreline_distance_m;
                lod.mid_surface_distance_m=2*lod.shoreline_distance_m;
                lod.max_triangle_budget=std::min(60000,lod.max_triangle_budget);
                const double seaRadius=planet.radius+planet.water.level_m/meters;
                const rendering::TerrainSurface sea({},lod,seaRadius,meters,{},0.0);
                water=sea.buildGeometryForEye(localEye,glm::dvec3(0.0));
            }
            return TimedTerrainBuild{std::move(land),std::move(water),
                std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()};
        };
        if (asyncWalking && meshReady[i] && localMask == lastLocalMask[i]) {
            pendingTerrain[i].eyeRadial = radial;
            pendingTerrain[i].localMask = localMask;
            pendingTerrain[i].geometry = std::async(std::launch::async,
                [trace=&cpuTrace, build=std::move(buildMeshes)]() {
                    CpuTrace::Thread thread(trace, "terrain worker");
                    return build();
                });
            continue;
        }
        auto built=buildMeshes();
        profiler.terrainBuild(built.milliseconds);
        if (built.water) {
            meshes.waterMeshes[i].loadTerrain(std::move(*built.water));
        }
        installLandMesh(i, std::move(built.geometry), radial, localMask);
    }
}

std::vector<std::uint64_t> Renderer::Impl::geometryRevisions() const {
    std::vector<std::uint64_t> result;
    for (const auto& mesh : meshes.planetMeshes) result.push_back(mesh.revision);
    return result;
}
}
