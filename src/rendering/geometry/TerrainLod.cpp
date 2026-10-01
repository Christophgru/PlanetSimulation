#include "rendering/geometry/Terrain.h"
#include "rendering/geometry/TerrainLod.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace rendering {
TerrainGeometry TerrainSurface::buildGeometryForEye(const glm::dvec3& eyeWorld,
                                    const glm::dvec3& planetCenter,
                                    const std::vector<int>* previousFaceZones,
                                    double zoneHysteresisMeters) const {
    CpuTrace::Scope scope("TerrainSurface::buildGeometryForEye");
    const glm::dvec3 offset = eyeWorld - planetCenter;
    const double cameraDistance = glm::length(offset);
    if (!std::isfinite(cameraDistance) || cameraDistance <= 0.0)
        throw std::invalid_argument("Terrain eye must be outside the planet center");
    const bool localView = cameraDistance < 3.0 * radius_;
    const glm::dvec3 eyeRadial = offset / cameraDistance;
    const bool nearWater = localView && waterLevelMeters_ && lod_.shoreline_edge_m > 0.0 &&
        std::abs((cameraDistance-radius_)*metersPerUnit_ - *waterLevelMeters_) < lod_.shoreline_distance_m;
    // Reserve local refinement space rather than exceeding the existing cap.
    const int minimumBase = 320 * 3 * lod_.base_edge_segments * (2*ringCount(lod_.base_edge_segments)-1);
    const int baseBudget = nearWater ? std::max(minimumBase, lod_.max_triangle_budget*3/4)
                                    : lod_.max_triangle_budget;

    std::vector<BaseFace> faces = baseFaces();
    if (!std::isfinite(zoneHysteresisMeters) || zoneHysteresisMeters < 0.0 ||
        (previousFaceZones && previousFaceZones->size() != faces.size()))
        throw std::invalid_argument("Invalid previous terrain zones or hysteresis");
    const TerrainLodBands bands(lod_);
    auto segmentsForZone = [&](int zone) { return bands.segments[zone]; };
    for (std::size_t index = 0; index < faces.size(); ++index) {
        auto& face = faces[index];
        face.distanceMeters = radius_ * metersPerUnit_ * std::acos(
            std::clamp(glm::dot(face.center, eyeRadial), -1.0, 1.0));
        const double faceReach = radius_ * metersPerUnit_ * std::max({
            std::acos(std::clamp(glm::dot(face.center, face.corners[0]), -1.0, 1.0)),
            std::acos(std::clamp(glm::dot(face.center, face.corners[1]), -1.0, 1.0)),
            std::acos(std::clamp(glm::dot(face.center, face.corners[2]), -1.0, 1.0))});
        const double closest = std::max(0.0, face.distanceMeters - faceReach);
        face.zone = localView ? bands.levelAt(closest) : 0;
        if (previousFaceZones) {
            const int previous = (*previousFaceZones)[index];
            if (previous < 0 || previous >= terrainLodCount)
                throw std::invalid_argument("Previous terrain level must be in [0, 7]");
            // Upgrade detail immediately; retain it while moving away
            // so a face cannot toggle near a distance boundary.
            if (localView && previous > 0 && closest <
                bands.outerDistanceMeters[previous - 1] + zoneHysteresisMeters)
                face.zone = std::max(face.zone, previous);
        }
        face.segments = segmentsForZone(face.zone);
        if (localView && face.zone > 0 &&
            lod_.steep_edge_segments > lod_.max_edge_segments &&
            maximumSlope(face) >= lod_.steep_slope_threshold) {
            face.steep = true;
            face.segments = lod_.steep_edge_segments;
        }
    }

    auto makeEdges = [&] {
        std::map<EdgeKey, EdgeInfo> edges;
        for (const auto& face : faces) {
            for (int side = 0; side < 3; ++side) {
                const glm::dvec3& a = face.corners[side];
                const glm::dvec3& b = face.corners[(side + 1) % 3];
                const VertexKey ak = vertexKey(a), bk = vertexKey(b);
                const EdgeKey key = ak < bk ? EdgeKey{ak, bk} : EdgeKey{bk, ak};
                auto [it, inserted] = edges.try_emplace(key);
                if (inserted) {
                    it->second.a = ak < bk ? a : b;
                    it->second.b = ak < bk ? b : a;
                }
                it->second.segments = std::max(it->second.segments, face.segments);
            }
        }
        return edges;
    };
    auto estimate = [&](const std::map<EdgeKey, EdgeInfo>& edges) {
        int triangles = 0;
        for (const auto& face : faces) {
            int border = 0;
            for (int side = 0; side < 3; ++side)
                border += edges.at(edgeKey(face.corners[side],
                                           face.corners[(side + 1) % 3])).segments;
            triangles += border * (2 * ringCount(face.segments) - 1);
        }
        return triangles;
    };
    auto edges = makeEdges();
    while (estimate(edges) > baseBudget) {
        auto candidate = faces.end();
        // Spend spare triangles on steep faces. Prefer near-zone crests
        // over middle-zone crests, and use the fixed face order within a
        // zone so small eye movements cannot swap tessellation between
        // equally important faces and make the ground pop.
        for (auto it = faces.begin(); it != faces.end(); ++it) {
            if (it->segments <= segmentsForZone(it->zone)) continue;
            if (candidate == faces.end() || it->zone < candidate->zone ||
                (it->zone == candidate->zone && it > candidate))
                candidate = it;
        }
        if (candidate != faces.end()) {
            candidate->segments = segmentsForZone(candidate->zone);
            edges = makeEdges();
            continue;
        }
        for (auto it = faces.begin(); it != faces.end(); ++it) {
            if (it->zone == 0) continue;
            if (candidate == faces.end() || it->zone < candidate->zone ||
                (it->zone == candidate->zone && it > candidate))
                candidate = it;
        }
        if (candidate == faces.end())
            throw std::invalid_argument("Terrain base mesh exceeds triangle budget");
        --candidate->zone;
        candidate->segments = segmentsForZone(candidate->zone);
        edges = makeEdges();
    }

    TerrainGeometry geometry;
    geometry.faceZones.reserve(faces.size());
    for (const auto& face : faces) {
        geometry.faceZones.push_back(face.zone);
        if (face.steep && face.segments > segmentsForZone(face.zone))
            ++geometry.steepRefinedFaces;
    }
    const int predicted = estimate(edges);
    geometry.vertices.reserve(static_cast<std::size_t>(predicted) * 27);
    geometry.indices.reserve(static_cast<std::size_t>(predicted) * 3);
    geometry.lodSinkMeters.reserve(static_cast<std::size_t>(predicted) * 3);
    // Bound inward displacement on tiny planets and near the center.
    const double maximumSink = std::min(lod_.sink_depth_m,
        (radius_ * metersPerUnit_ - totalAmplitudeMeters_) * 0.001);
    auto faceSink = [&](const BaseFace& face) { return bands.sinkMeters(face.segments, maximumSink); };
    std::map<VertexKey, double> cornerSinks;
    std::map<EdgeKey, double> edgeSinks;
    for (const auto& face : faces) {
        const double sink = faceSink(face);
        for (int side = 0; side < 3; ++side) {
            const auto& a = face.corners[side];
            const auto& b = face.corners[(side + 1) % 3];
            auto [corner, insertedCorner] = cornerSinks.try_emplace(vertexKey(a), sink);
            if (!insertedCorner) corner->second = std::min(corner->second, sink);
            auto [edge, insertedEdge] = edgeSinks.try_emplace(edgeKey(a, b), sink);
            if (!insertedEdge) edge->second = std::min(edge->second, sink);
        }
    }
    auto sampleAt = [&](const glm::dvec3& radial, double sink) {
        // Fixed height samples plus a level-dependent offset: unchanged levels
        // stay fixed when walking. The finest incident face owns shared heights.
        const double height = heightAt(radial);
        const double arcMeters = radius_ * metersPerUnit_ * std::acos(
            std::clamp(glm::dot(radial, eyeRadial), -1.0, 1.0));
        if (localView && arcMeters < lod_.mid_surface_distance_m)
            ++geometry.fineNoiseSamples;
        else ++geometry.coarseNoiseSamples;
        auto sample = makeGridSample(radial, height);
        sample.sinkMeters = sink;
        return sample;
    };
    for (auto& [key, edge] : edges) {
        edge.samples.reserve(edge.segments + 1);
        for (int step = 0; step <= edge.segments; ++step) {
            const double t = static_cast<double>(step) / edge.segments;
            // Shared corners use the finest of all incident faces; edge
            // interiors use the finer neighbor. Smooth interpolation joins them.
            const double sink = t <= 0.5 ?
                std::lerp(cornerSinks.at(vertexKey(edge.a)), edgeSinks.at(key), smoothstep(0, 0.5, t)) :
                std::lerp(edgeSinks.at(key), cornerSinks.at(vertexKey(edge.b)), smoothstep(0.5, 1, t));
            edge.samples.push_back(sampleAt(glm::normalize((1.0 - t) * edge.a + t * edge.b), sink));
        }
    }
    for (const auto& face : faces) {
        ++geometry.lodFaces[face.zone];
        ++geometry.zoneFaces[face.zone == 7 ? 2 : face.zone > 0 ? 1 : 0];
        std::vector<GridSample> boundary;
        for (int side = 0; side < 3; ++side) {
            const auto& a = face.corners[side];
            const auto& b = face.corners[(side + 1) % 3];
            const auto& edge = edges.at(edgeKey(a, b));
            const bool forward = vertexKey(a) < vertexKey(b);
            for (int step = 0; step < edge.segments; ++step)
                boundary.push_back(edge.samples[forward ? step : edge.segments - step]);
        }
        const int count = static_cast<int>(boundary.size());
        const int rings = ringCount(face.segments);
        std::vector<GridSample> previous;
        const GridSample center = sampleAt(face.center, faceSink(face));
        for (int ring = 1; ring <= rings; ++ring) {
            std::vector<GridSample> current;
            current.reserve(count);
            const double fraction = static_cast<double>(ring) / rings;
            for (const auto& outer : boundary) {
                if (ring == rings) {
                    current.push_back(outer);
                } else {
                    const glm::dvec3 radial = glm::normalize(
                        (1.0 - fraction) * face.center +
                        fraction * glm::normalize(outer.position));
                    current.push_back(sampleAt(radial, std::lerp(center.sinkMeters,
                        outer.sinkMeters, smoothstep(0, 1, fraction))));
                }
            }
            for (int i = 0; i < count; ++i) {
                const int next = (i + 1) % count;
                if (ring == 1) {
                    emitFace(center, current[i], current[next], geometry);
                } else {
                    emitFace(previous[i], current[i], current[next], geometry);
                    emitFace(previous[i], current[next], previous[next], geometry);
                }
            }
            previous = std::move(current);
        }
    }
    if (geometry.triangleCount() != predicted)
        throw std::logic_error("Terrain triangle budget estimation disagrees with mesh");
    if (nearWater) refineShoreline(geometry, offset/radius_);
    // Apply the offset after refinement, which interpolates the same scalar
    // field. Exactly one closed surface is submitted; no buried LOD draws.
    for (std::size_t i = 0; i < geometry.lodSinkMeters.size(); ++i) {
        if (geometry.lodSinkMeters[i] == 0.0f) continue;
        const std::size_t v = i * 9;
        const glm::dvec3 p(geometry.vertices[v], geometry.vertices[v+1], geometry.vertices[v+2]);
        const glm::dvec3 sunk = p - glm::normalize(p) *
            (geometry.lodSinkMeters[i] / (radius_ * metersPerUnit_));
        for (int axis = 0; axis < 3; ++axis) geometry.vertices[v+axis] = static_cast<float>(sunk[axis]);
    }
    return geometry;
}

} // namespace rendering
