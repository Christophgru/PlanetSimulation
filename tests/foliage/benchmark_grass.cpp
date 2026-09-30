#include "app/scene/PreparedScene.h"
#include "config/Config.h"
#include "config/SceneReplay.h"
#include "rendering/foliage/GrassPlacement.h"
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
        std::cout << "iteration,placement_ms,placement_cpu_ms,blades,checksum\n" << std::setprecision(9);
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
            std::cout << iteration << ',' << ms << ',' << cpuMs << ',' << blades.size() << ',' << hash << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
