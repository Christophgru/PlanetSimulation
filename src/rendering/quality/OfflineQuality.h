#pragma once
#include "app/CommandLineOptions.h"
#include "config/ScenarioConfig.h"
#include <algorithm>
#include <cmath>

namespace rendering {
// Derived capture settings stay separate from the validated, saved scene.
// Extended radius uses the same single layer and planet-local placement rules.
inline config::ScenarioConfig offlineScenario(config::ScenarioConfig scene,
                                              const app::CommandLineOptions& options) {
    if (!options.offlineQuality) return scene;
    if (!options.renderTestMode)
        throw std::invalid_argument("Offline quality requires a capture output");
    if (!std::isfinite(options.foliageDistanceMultiplier) ||
        options.foliageDistanceMultiplier<1 || options.foliageDistanceMultiplier>20)
        throw std::invalid_argument("Offline foliage distance multiplier must be in 1..20");
    for (std::size_t i=0;i<scene.planets.size();++i) {
        auto& planet=scene.planets[i];
        auto& grass=planet.foliage;
        grass.draw_distance_m *= options.foliageDistanceMultiplier;
        // Area scales quadratically. Cap automatic growth at two million
        // candidates (~256 MB for the two compute queues), retaining larger
        // explicit user budgets. Hard budgets still govern actual coverage.
        if (grass.enabled) grass.max_blades=std::max(grass.max_blades,
            static_cast<int>(std::min(2000000.0, grass.max_blades *
                options.foliageDistanceMultiplier * options.foliageDistanceMultiplier)));
        auto& lod=planet.terrain_lod;
        if (!scene.surface_camera.enabled || i!=static_cast<std::size_t>(scene.surface_camera.planet_index))
            lod.base_edge_segments=std::max(lod.base_edge_segments,10);
        lod.medium_edge_segments=std::max(lod.medium_edge_segments,16);
        lod.max_edge_segments=32;
        lod.steep_edge_segments=32;
        lod.max_triangle_budget=100000;
        lod.sink_depth_m=0;
        lod.validate();
    }
    return scene;
}
inline int sunSphereSegments(bool offline) { return offline ? 256 : 32; }
}
