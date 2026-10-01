#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include "config/ScenarioConfig.h"

namespace rendering {
inline constexpr int terrainLodCount = 8;

// Level 0 is distant, level 7 is finest. Keep the old three config anchors;
// intermediate levels are derived rather than adding seven tunable distances.
struct TerrainLodBands {
    std::array<int, terrainLodCount> segments{};
    std::array<double, terrainLodCount - 1> outerDistanceMeters{};

    explicit TerrainLodBands(const config::PlanetConfig::TerrainLod& lod) {
        for (int level = 0; level < terrainLodCount; ++level) {
            const double value = level <= 3 ?
                std::lerp(double(lod.base_edge_segments), double(lod.medium_edge_segments), level / 3.0) :
                std::lerp(double(lod.medium_edge_segments), double(lod.max_edge_segments), (level - 3) / 4.0);
            segments[level] = static_cast<int>(std::lround(value));
            if (level > 0)
                outerDistanceMeters[level - 1] = std::lerp(lod.mid_surface_distance_m,
                    lod.near_surface_distance_m, (level - 1) / 6.0);
        }
    }

    int levelAt(double distanceMeters) const {
        int level = 0;
        while (level < terrainLodCount - 1 && distanceMeters < outerDistanceMeters[level]) ++level;
        return level;
    }

    double sinkMeters(int edgeSegments, double maximumSinkMeters) const {
        if (segments.back() == segments.front()) return 0.0;
        const double coarseness = std::clamp(double(segments.back() - edgeSegments) /
            (segments.back() - segments.front()), 0.0, 1.0);
        return maximumSinkMeters * coarseness * coarseness;
    }
};
} // namespace rendering
