#include "app/scene/PreparedScene.h"
#include "config/Config.h"
#include "config/SceneReplay.h"
#include "rendering/foliage/GrassPlacement.h"
#include "rendering/foliage/GrassLod.h"
#include "rendering/foliage/horizon/HorizonGrassPlan.h"
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>

// Manual CPU benchmark, deliberately not a timing-sensitive CTest assertion.
// Input is the self-contained input.json emitted by camera_movement.py.
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Usage: foliage_benchmark REPLAY.json\n"; return 1; }
    try {
        const auto replay = config::Config::load(argv[1]).data();
        auto document = config::applyCameraReplay(replay.at("scenario"), replay);
        app::PreparedScene scene{config::ScenarioConfig{config::Config{std::move(document)}}};
        scene.updateSimulation(config::replayStartTime(scene.scenario, std::nullopt));
        const auto index = scene.scenario.surface_camera.planet_index;
        const auto& planet = scene.scenario.planets.at(index);
        const auto localEye = scene.bodies.at(index + 1).toLocalPoint(scene.surfaceCamera->position());
        const auto mesh = scene.terrainSurfaces.at(index).buildGeometryForEye(localEye, glm::dvec3(0));
        std::cout << "iteration,placement_ms,placement_cpu_ms,blades,checksum,lod_batch_ms,old_vertices,old_triangles,old_instance_bytes,new_blades,new_vertices,new_triangles,new_instance_bytes,new_batches,horizon_plan_ms,horizon_cpu_ms,horizon_patches,horizon_candidates,horizon_patch_bytes\n" << std::setprecision(9);
        for (int iteration = 0; iteration < 12; ++iteration) {
            const auto start = std::chrono::steady_clock::now();
            const auto cpuStart = std::clock();
            const auto blades = rendering::placeGrass(mesh.vertices, mesh.indices, planet,
                scene.scenario.metersPerWorldUnit(), localEye / planet.radius);
            const double cpuMs = 1000.0 * (std::clock() - cpuStart) / CLOCKS_PER_SEC;
            const double ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
            // Hash explicit float components, never struct padding.
            std::uint64_t hash = 14695981039346656037ULL;
            const auto append = [&](float value) {
                std::uint32_t bits;
                std::memcpy(&bits, &value, sizeof(bits));
                for (unsigned shift = 0; shift < 32; shift += 8) {
                    hash ^= (bits >> shift) & 255u;
                    hash *= 1099511628211ULL;
                }
            };
            for (const auto& blade : blades) {
                for (int i=0; i<3; ++i) append(blade.root[i]);
                for (int i=0; i<3; ++i) append(blade.up[i]);
                for (int i=0; i<4; ++i) append(blade.variation[i]);
            }
            const double scale=planet.radius*scene.scenario.metersPerWorldUnit();
            const auto eye=localEye/planet.radius;
            const double margin=rendering::grassRebuildDistance(planet.foliage);
            std::size_t oldVertices=0,oldTriangles=0;
            for (const auto& blade:blades) {
                const bool near=glm::length(glm::dvec3(blade.root)-eye)*scale<=15+margin;
                oldVertices+=near ? 14 : 4;
                oldTriangles+=near ? 12 : 2; // Includes the old degenerate tip.
            }
            const auto batchStart=std::chrono::steady_clock::now();
            const auto plan=rendering::batchGrass(blades,eye,scale,planet.foliage.draw_distance_m,margin);
            const double batchMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-batchStart).count();
            std::size_t newVertices=0,newTriangles=0,newBatches=0;
            int previousSegments=0;
            for (int level=0; level<8; ++level) {
                const auto count=plan.batches[level].count;
                newVertices+=count*rendering::grassLodVertices(level);
                newTriangles+=count*(rendering::grassLodVertices(level)-2);
                if (count) {
                    newBatches+=rendering::grassLodSegments[level]!=previousSegments;
                    previousSegments=rendering::grassLodSegments[level];
                }
            }
            const auto horizonStart=std::chrono::steady_clock::now();
            const auto horizonCpuStart=std::clock();
            const auto horizon=rendering::planHorizonGrass(mesh.vertices,mesh.indices,planet,
                scene.scenario.metersPerWorldUnit(),eye);
            const double horizonCpuMs=1000.0*(std::clock()-horizonCpuStart)/CLOCKS_PER_SEC;
            const double horizonMs=std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now()-horizonStart).count();
            std::cout << iteration << ',' << ms << ',' << cpuMs << ',' << blades.size() << ',' << hash
                      << ',' << batchMs << ',' << oldVertices << ',' << oldTriangles << ',' << blades.size()*sizeof(rendering::GrassBlade)
                      << ',' << plan.blades.size() << ',' << newVertices << ',' << newTriangles << ',' << plan.blades.size()*sizeof(rendering::GrassBlade)
                      << ',' << newBatches << ',' << horizonMs << ',' << horizonCpuMs
                      << ',' << horizon.patches.size() << ',' << horizon.candidates
                      << ',' << horizon.patches.size()*sizeof(rendering::HorizonGrassPatch) << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
