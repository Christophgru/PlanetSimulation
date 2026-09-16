#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <glm/glm.hpp>
#include "config/ScenarioConfig.h"

namespace rendering {

// Positions and colors are interleaved as position, face normal, color factor.
// Faces own vertices, but shared corners receive the same sampled height tint;
// the shader interpolates those tints across the triangle boundaries.
struct TerrainGeometry {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::array<int, 3> zoneFaces{}; // far, middle, near
    std::vector<int> faceZones; // one zone per fixed base face, for LOD hysteresis
    int steepRefinedFaces = 0;
    int coarseNoiseSamples = 0;
    int fineNoiseSamples = 0;
    int triangleCount() const { return static_cast<int>(indices.size() / 3); }
};

class TerrainSurface {
public:
    TerrainSurface(const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions,
                   const config::PlanetConfig::TerrainLod& lod,
                   double radiusWorld, double metersPerWorldUnit,
                   const config::PlanetConfig::TerrainLandscape& landscape = {})
        : functions_(functions), lod_(lod), landscape_(landscape), radius_(radiusWorld),
          metersPerUnit_(metersPerWorldUnit) {
        lod_.validate();
        landscape_.validate();
        for (const auto& function : functions_) {
            function.validate();
            totalAmplitudeMeters_ += function.amplitude_m;
        }
        totalAmplitudeMeters_ += landscape_.maximumAbsoluteHeightMeters();
        if (!std::isfinite(radius_) || radius_ <= 0.0 ||
            !std::isfinite(metersPerUnit_) || metersPerUnit_ <= 0.0 ||
            !std::isfinite(totalAmplitudeMeters_) ||
            totalAmplitudeMeters_ >= radius_ * metersPerUnit_ ||
            functions_.size() > 8) {
            throw std::invalid_argument("Invalid terrain radius or amplitude");
        }
    }

    double heightAt(const glm::dvec3& radial) const {
        const double length = glm::length(radial);
        if (!std::isfinite(length) || length <= 0.0) {
            throw std::invalid_argument("Terrain height needs a finite radial direction");
        }
        const glm::dvec3 direction = radial / length;
        return heightMeters(direction, 1.0) / metersPerUnit_;
    }

    double regionPlainWeight(const glm::dvec3& radial) const {
        if (!landscape_.enabled) return 0.0;
        const double sample = valueNoise(glm::normalize(radial) * 2.7, landscape_.seed + 1);
        return 1.0 - smoothstep(landscape_.plain_threshold - 0.06,
                                landscape_.plain_threshold + 0.06, sample);
    }

    double regionCliffWeight(const glm::dvec3& radial) const {
        if (!landscape_.enabled) return 0.0;
        const double sample = valueNoise(glm::normalize(radial) * 2.4, landscape_.seed + 2);
        return smoothstep(landscape_.cliff_threshold - 0.06,
                          landscape_.cliff_threshold + 0.06, sample);
    }

private:
    static double smoothstep(double low, double high, double value) {
        const double t = std::clamp((value - low) / (high - low), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

    double heightMeters(const glm::dvec3& direction, double detailWeight) const {
        double height = 0.0;
        double detailMask = 1.0;
        if (landscape_.enabled) {
            const double continent = 2.0 * valueNoise(
                direction * landscape_.continent_frequency, landscape_.seed) - 1.0;
            const double plain = regionPlainWeight(direction);
            const double cliff = regionCliffWeight(direction);
            height = landscape_.elevation_offset_m +
                     landscape_.continent_amplitude_m * continent * (1.0 - 0.8 * plain);
            const double ridgeSignal = 2.0 * valueNoise(
                direction * landscape_.cliff_frequency, landscape_.seed + 3) - 1.0;
            double ridgeDistance = std::abs(ridgeSignal);
            if (landscape_.ridge_smoothing > 0.0) {
                const double smoothing = landscape_.ridge_smoothing;
                const double scale = std::sqrt(1.0 + smoothing * smoothing) - smoothing;
                ridgeDistance = (std::sqrt(ridgeSignal * ridgeSignal +
                                           smoothing * smoothing) - smoothing) / scale;
            }
            const double ridge = 1.0 - std::clamp(ridgeDistance, 0.0, 1.0);
            height += landscape_.cliff_amplitude_m * cliff * std::pow(ridge, 5.0);
            detailMask = 1.0 - 0.9 * plain;
        }
        for (const auto& function : functions_) {
            if (function.amplitude_m == 0.0) continue;
            double frequency = function.frequency;
            double weight = 1.0;
            double full = 0.0;
            double weightSum = 0.0;
            double broad = 0.0;
            for (int octave = 0; octave < function.octaves; ++octave) {
                if (octave > 0 && detailWeight <= 0.0) break;
                const double signedValue =
                    2.0 * valueNoise(direction * frequency, function.seed) - 1.0;
                const double sample = function.type == "ridged_fbm" ?
                    1.0 - 2.0 * std::abs(signedValue) : signedValue;
                if (octave == 0) broad = sample;
                full += weight * sample;
                weightSum += weight;
                frequency *= function.lacunarity;
                weight *= function.persistence;
            }
            const double mixed = broad + detailWeight * (full / weightSum - broad);
            height += detailMask * function.amplitude_m * mixed;
        }
        return height;
    }

public:

    int lodLevel(double cameraDistanceWorld) const {
        if (!std::isfinite(cameraDistanceWorld) || cameraDistanceWorld < 0.0) {
            throw std::invalid_argument("Invalid camera distance for terrain LOD");
        }
        const double distanceDiameters = cameraDistanceWorld / (2.0 * radius_);
        if (distanceDiameters <= lod_.lod_near_diameters)
            return lod_.max_edge_segments;
        if (distanceDiameters >= lod_.lod_far_diameters)
            return lod_.base_edge_segments;
        const double portion = (lod_.lod_far_diameters - distanceDiameters) /
            (lod_.lod_far_diameters - lod_.lod_near_diameters);
        const int steps = lod_.max_edge_segments - lod_.base_edge_segments;
        return lod_.base_edge_segments +
               static_cast<int>(std::lround(portion * steps));
    }

    TerrainGeometry buildGeometry(int edgeSegments) const {
        if (edgeSegments < 1 || edgeSegments > lod_.max_edge_segments) {
            throw std::invalid_argument("Terrain edge segments exceed configured cap");
        }
        TerrainGeometry geometry;
        const int triangles = 320 * edgeSegments * edgeSegments;
        geometry.vertices.reserve(static_cast<std::size_t>(triangles) * 3 * 9);
        geometry.indices.reserve(static_cast<std::size_t>(triangles) * 3);

        const double t = (1.0 + std::sqrt(5.0)) / 2.0;
        const std::array<glm::dvec3, 12> raw = {{
            {-1,t,0}, {1,t,0}, {-1,-t,0}, {1,-t,0},
            {0,-1,t}, {0,1,t}, {0,-1,-t}, {0,1,-t},
            {t,0,-1}, {t,0,1}, {-t,0,-1}, {-t,0,1}
        }};
        const std::array<std::array<int, 3>, 20> faces = {{
            {{0,11,5}}, {{0,5,1}}, {{0,1,7}}, {{0,7,10}}, {{0,10,11}},
            {{1,5,9}}, {{5,11,4}}, {{11,10,2}}, {{10,7,6}}, {{7,1,8}},
            {{3,9,4}}, {{3,4,2}}, {{3,2,6}}, {{3,6,8}}, {{3,8,9}},
            {{4,9,5}}, {{2,4,11}}, {{6,2,10}}, {{8,6,7}}, {{9,8,1}}
        }};
        for (const auto& face : faces) {
            const glm::dvec3 a = glm::normalize(raw[face[0]]);
            glm::dvec3 b = glm::normalize(raw[face[1]]);
            glm::dvec3 c = glm::normalize(raw[face[2]]);
            if (glm::dot(glm::cross(b - a, c - a), a + b + c) < 0.0)
                std::swap(b, c);
            subdivideBase(a, b, c, 2, edgeSegments, geometry);
        }
        return geometry;
    }

    // Tessellate only the faces close to the camera. Every base-face edge is
    // sampled once at the finer of its two adjacent zones, then both faces
    // use those identical boundary samples. Concentric interior rings fill
    // unequal edge segment counts without T junctions or cracks.
    TerrainGeometry buildGeometryForEye(const glm::dvec3& eyeWorld,
                                        const glm::dvec3& planetCenter,
                                        const std::vector<int>* previousFaceZones = nullptr,
                                        double zoneHysteresisMeters = 0.0) const {
        const glm::dvec3 offset = eyeWorld - planetCenter;
        const double cameraDistance = glm::length(offset);
        if (!std::isfinite(cameraDistance) || cameraDistance <= 0.0)
            throw std::invalid_argument("Terrain eye must be outside the planet center");
        const bool localView = cameraDistance < 3.0 * radius_;
        const glm::dvec3 eyeRadial = offset / cameraDistance;

        std::vector<BaseFace> faces = baseFaces();
        if (!std::isfinite(zoneHysteresisMeters) || zoneHysteresisMeters < 0.0 ||
            (previousFaceZones && previousFaceZones->size() != faces.size()))
            throw std::invalid_argument("Invalid previous terrain zones or hysteresis");
        auto segmentsForZone = [&](int zone) {
            return zone == 2 ? lod_.max_edge_segments :
                   zone == 1 ? lod_.medium_edge_segments : lod_.base_edge_segments;
        };
        for (std::size_t index = 0; index < faces.size(); ++index) {
            auto& face = faces[index];
            face.distanceMeters = radius_ * metersPerUnit_ * std::acos(
                std::clamp(glm::dot(face.center, eyeRadial), -1.0, 1.0));
            const double faceReach = radius_ * metersPerUnit_ * std::max({
                std::acos(std::clamp(glm::dot(face.center, face.corners[0]), -1.0, 1.0)),
                std::acos(std::clamp(glm::dot(face.center, face.corners[1]), -1.0, 1.0)),
                std::acos(std::clamp(glm::dot(face.center, face.corners[2]), -1.0, 1.0))});
            const double closest = std::max(0.0, face.distanceMeters - faceReach);
            face.zone = !localView || closest >= lod_.mid_surface_distance_m ? 0 :
                        closest >= lod_.near_surface_distance_m ? 1 : 2;
            if (localView && previousFaceZones) {
                const int previous = (*previousFaceZones)[index];
                if (previous < 0 || previous > 2)
                    throw std::invalid_argument("Previous terrain zone must be 0, 1 or 2");
                // Upgrade detail immediately; retain it while moving away
                // so a face cannot toggle near a distance boundary.
                if (previous == 2 && closest <
                    lod_.near_surface_distance_m + zoneHysteresisMeters)
                    face.zone = 2;
                else if (previous >= 1 && closest <
                    lod_.mid_surface_distance_m + zoneHysteresisMeters)
                    face.zone = std::max(face.zone, 1);
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
        while (estimate(edges) > lod_.max_triangle_budget) {
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
        auto sampleAt = [&](const glm::dvec3& radial) {
            // A vertex's elevation belongs to the planet, not to the eye.
            // Camera-relative detail weights made the same ground rise and
            // fall every time the walking camera triggered a mesh rebuild.
            const double height = heightAt(radial);
            const double arcMeters = radius_ * metersPerUnit_ * std::acos(
                std::clamp(glm::dot(radial, eyeRadial), -1.0, 1.0));
            if (localView && arcMeters < lod_.mid_surface_distance_m)
                ++geometry.fineNoiseSamples;
            else ++geometry.coarseNoiseSamples;
            return GridSample{radial * (1.0 + height / radius_), height,
                              colorAt(radial, height)};
        };
        for (auto& [key, edge] : edges) {
            edge.samples.reserve(edge.segments + 1);
            for (int step = 0; step <= edge.segments; ++step) {
                const double t = static_cast<double>(step) / edge.segments;
                edge.samples.push_back(sampleAt(glm::normalize((1.0 - t) * edge.a + t * edge.b)));
            }
        }
        for (const auto& face : faces) {
            ++geometry.zoneFaces[face.zone];
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
            const GridSample center = sampleAt(face.center);
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
                        current.push_back(sampleAt(radial));
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
        return geometry;
    }

    const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions() const {
        return functions_;
    }
    const config::PlanetConfig::TerrainLod& lodSettings() const { return lod_; }

private:
    struct GridSample {
        glm::dvec3 position;
        double height;
        glm::dvec3 color;
    };
    using VertexKey = std::array<std::int64_t, 3>;
    using EdgeKey = std::pair<VertexKey, VertexKey>;
    struct BaseFace {
        std::array<glm::dvec3, 3> corners;
        glm::dvec3 center;
        double distanceMeters = 0.0;
        int zone = 0;
        int segments = 1;
        bool steep = false;
    };
    struct EdgeInfo {
        glm::dvec3 a{0.0}, b{0.0};
        int segments = 0;
        std::vector<GridSample> samples;
    };
    static VertexKey vertexKey(const glm::dvec3& p) {
        return {std::llround(p.x * 1e10), std::llround(p.y * 1e10),
                std::llround(p.z * 1e10)};
    }
    static EdgeKey edgeKey(const glm::dvec3& a, const glm::dvec3& b) {
        const VertexKey ak = vertexKey(a), bk = vertexKey(b);
        return ak < bk ? EdgeKey{ak, bk} : EdgeKey{bk, ak};
    }
    static int ringCount(int segments) { return std::max(1, (segments + 1) / 2); }

    double maximumSlope(const BaseFace& face) const {
        std::vector<glm::dvec3> samples;
        samples.reserve(10);
        samples.push_back(face.center);
        for (const auto& corner : face.corners) samples.push_back(corner);
        for (int side = 0; side < 3; ++side) {
            const auto& a = face.corners[side];
            const auto& b = face.corners[(side + 1) % 3];
            samples.push_back(glm::normalize(2.0 * a + b));
            samples.push_back(glm::normalize(a + 2.0 * b));
        }
        std::vector<double> heights;
        heights.reserve(samples.size());
        for (const auto& sample : samples)
            heights.push_back(heightAt(sample) * metersPerUnit_);
        double maximum = 0.0;
        for (std::size_t a = 0; a < samples.size(); ++a) {
            for (std::size_t b = a + 1; b < samples.size(); ++b) {
                const double distance = radius_ * metersPerUnit_ * std::acos(
                    std::clamp(glm::dot(samples[a], samples[b]), -1.0, 1.0));
                if (distance > 1e-9)
                    maximum = std::max(maximum,
                        std::abs(heights[a] - heights[b]) / distance);
            }
        }
        return maximum;
    }

    static void collectBase(const glm::dvec3& a, const glm::dvec3& b,
                            const glm::dvec3& c, int remaining,
                            std::vector<BaseFace>& out) {
        if (remaining == 0) {
            out.push_back({{a, b, c}, glm::normalize(a + b + c)});
            return;
        }
        const glm::dvec3 ab = glm::normalize(a + b);
        const glm::dvec3 bc = glm::normalize(b + c);
        const glm::dvec3 ca = glm::normalize(c + a);
        collectBase(a, ab, ca, remaining - 1, out);
        collectBase(b, bc, ab, remaining - 1, out);
        collectBase(c, ca, bc, remaining - 1, out);
        collectBase(ab, bc, ca, remaining - 1, out);
    }

    static std::vector<BaseFace> baseFaces() {
        const double t = (1.0 + std::sqrt(5.0)) / 2.0;
        const std::array<glm::dvec3, 12> raw = {{
            {-1,t,0}, {1,t,0}, {-1,-t,0}, {1,-t,0},
            {0,-1,t}, {0,1,t}, {0,-1,-t}, {0,1,-t},
            {t,0,-1}, {t,0,1}, {-t,0,-1}, {-t,0,1}
        }};
        const std::array<std::array<int, 3>, 20> indices = {{
            {{0,11,5}}, {{0,5,1}}, {{0,1,7}}, {{0,7,10}}, {{0,10,11}},
            {{1,5,9}}, {{5,11,4}}, {{11,10,2}}, {{10,7,6}}, {{7,1,8}},
            {{3,9,4}}, {{3,4,2}}, {{3,2,6}}, {{3,6,8}}, {{3,8,9}},
            {{4,9,5}}, {{2,4,11}}, {{6,2,10}}, {{8,6,7}}, {{9,8,1}}
        }};
        std::vector<BaseFace> result;
        result.reserve(320);
        for (const auto& face : indices) {
            const glm::dvec3 a = glm::normalize(raw[face[0]]);
            glm::dvec3 b = glm::normalize(raw[face[1]]);
            glm::dvec3 c = glm::normalize(raw[face[2]]);
            if (glm::dot(glm::cross(b - a, c - a), a + b + c) < 0.0)
                std::swap(b, c);
            collectBase(a, b, c, 2, result);
        }
        return result;
    }

    static std::uint32_t hash(int x, int y, int z, int seed) {
        std::uint32_t h = static_cast<std::uint32_t>(seed);
        h ^= static_cast<std::uint32_t>(x) * 0x9e3779b1u;
        h ^= static_cast<std::uint32_t>(y) * 0x85ebca77u;
        h ^= static_cast<std::uint32_t>(z) * 0xc2b2ae3du;
        h ^= h >> 16;
        h *= 0x7feb352du;
        h ^= h >> 15;
        h *= 0x846ca68bu;
        h ^= h >> 16;
        return h;
    }

    double valueNoise(const glm::dvec3& point, int seed) const {
        const int ix = static_cast<int>(std::floor(point.x));
        const int iy = static_cast<int>(std::floor(point.y));
        const int iz = static_cast<int>(std::floor(point.z));
        auto smooth = [](double f) { return f * f * (3.0 - 2.0 * f); };
        const glm::dvec3 f(smooth(point.x - ix), smooth(point.y - iy),
                           smooth(point.z - iz));
        double result = 0.0;
        for (int z = 0; z <= 1; ++z) {
            for (int y = 0; y <= 1; ++y) {
                for (int x = 0; x <= 1; ++x) {
                    const double value = hash(ix + x, iy + y, iz + z,
                                              seed) / 4294967295.0;
                    result += value * (x ? f.x : 1.0 - f.x) *
                                      (y ? f.y : 1.0 - f.y) *
                                      (z ? f.z : 1.0 - f.z);
                }
            }
        }
        return result;
    }

    glm::dvec3 colorAt(const glm::dvec3& radial, double heightWorld) const {
        if (!landscape_.enabled) {
            const double normalizedHeight = totalAmplitudeMeters_ == 0.0 ? 0.0 :
                heightWorld / (totalAmplitudeMeters_ / metersPerUnit_);
            const double tint = 0.72 + 0.45 * normalizedHeight;
            return glm::dvec3(tint);
        }
        const double heightMeters = heightWorld * metersPerUnit_;
        const double plain = regionPlainWeight(radial);
        const double cliff = regionCliffWeight(radial);
        const glm::dvec3 grass(1.70, 1.50, 0.26);
        const glm::dvec3 rock(2.80, 1.48, 0.62);
        const glm::dvec3 seabed(0.30, 0.40, 0.19);
        const glm::dvec3 land = glm::mix(grass, rock,
            std::clamp(cliff * (1.0 - 0.35 * plain), 0.0, 1.0));
        const double submerged = 1.0 - smoothstep(-2.0, 0.5, heightMeters);
        return glm::mix(land, seabed, submerged);
    }

    void subdivideBase(const glm::dvec3& a, const glm::dvec3& b,
                       const glm::dvec3& c, int remaining,
                       int edgeSegments, TerrainGeometry& geometry) const {
        if (remaining == 0) {
            emitGrid(a, b, c, edgeSegments, geometry);
            return;
        }
        const glm::dvec3 ab = glm::normalize(a + b);
        const glm::dvec3 bc = glm::normalize(b + c);
        const glm::dvec3 ca = glm::normalize(c + a);
        subdivideBase(a, ab, ca, remaining - 1, edgeSegments, geometry);
        subdivideBase(b, bc, ab, remaining - 1, edgeSegments, geometry);
        subdivideBase(c, ca, bc, remaining - 1, edgeSegments, geometry);
        subdivideBase(ab, bc, ca, remaining - 1, edgeSegments, geometry);
    }

    void emitGrid(const glm::dvec3& a, const glm::dvec3& b,
                  const glm::dvec3& c, int segments,
                  TerrainGeometry& geometry) const {
        std::vector<std::vector<GridSample>> grid(segments + 1);
        for (int i = 0; i <= segments; ++i) {
            for (int j = 0; i + j <= segments; ++j) {
                const glm::dvec3 radial = glm::normalize(
                    static_cast<double>(segments - i - j) * a +
                    static_cast<double>(i) * b + static_cast<double>(j) * c);
                const double height = heightAt(radial);
                grid[i].push_back({radial * (1.0 + height / radius_), height,
                                   colorAt(radial, height)});
            }
        }
        for (int i = 0; i < segments; ++i) {
            for (int j = 0; i + j < segments; ++j) {
                emitFace(grid[i][j], grid[i + 1][j], grid[i][j + 1], geometry);
                if (i + j + 1 < segments) {
                    emitFace(grid[i + 1][j], grid[i + 1][j + 1],
                             grid[i][j + 1], geometry);
                }
            }
        }
    }

    void emitFace(const GridSample& a, const GridSample& b,
                  const GridSample& c, TerrainGeometry& geometry) const {
        const GridSample* first = &a;
        const GridSample* second = &b;
        const GridSample* third = &c;
        const glm::dvec3& pa = a.position;
        const glm::dvec3& pb = b.position;
        const glm::dvec3& pc = c.position;
        if (glm::dot(glm::cross(pb - pa, pc - pa), pa + pb + pc) < 0.0)
            std::swap(second, third);
        const glm::dvec3 normal = glm::normalize(glm::cross(
            second->position - first->position, third->position - first->position));
        const unsigned int start = static_cast<unsigned int>(geometry.indices.size());
        for (const GridSample* sample : {first, second, third}) {
            for (double component : {sample->position.x, sample->position.y,
                                     sample->position.z})
                geometry.vertices.push_back(static_cast<float>(component));
            for (double component : {normal.x, normal.y, normal.z})
                geometry.vertices.push_back(static_cast<float>(component));
            for (double component : {sample->color.x, sample->color.y, sample->color.z})
                geometry.vertices.push_back(static_cast<float>(component));
        }
        geometry.indices.insert(geometry.indices.end(), {start, start + 1, start + 2});
    }

    std::vector<config::PlanetConfig::SurfaceNoiseFunction> functions_;
    config::PlanetConfig::TerrainLod lod_;
    config::PlanetConfig::TerrainLandscape landscape_;
    double radius_;
    double metersPerUnit_;
    double totalAmplitudeMeters_ = 0.0;
};

} // namespace rendering
