#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
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
    int triangleCount() const { return static_cast<int>(indices.size() / 3); }
};

class TerrainSurface {
public:
    TerrainSurface(const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions,
                   const config::PlanetConfig::TerrainLod& lod,
                   double radiusWorld, double metersPerWorldUnit)
        : functions_(functions), lod_(lod), radius_(radiusWorld),
          metersPerUnit_(metersPerWorldUnit) {
        lod_.validate();
        for (const auto& function : functions_) {
            function.validate();
            totalAmplitudeMeters_ += function.amplitude_m;
        }
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
        if (totalAmplitudeMeters_ == 0.0) return 0.0;
        const glm::dvec3 direction = radial / length;
        double heightMeters = 0.0;
        for (const auto& function : functions_) {
            if (function.amplitude_m == 0.0) continue;
            double frequency = function.frequency;
            double weight = 1.0;
            double total = 0.0;
            double weightSum = 0.0;
            for (int octave = 0; octave < function.octaves; ++octave) {
                const double signedValue =
                    2.0 * valueNoise(direction * frequency, function.seed) - 1.0;
                const double sample = function.type == "ridged_fbm" ?
                    1.0 - 2.0 * std::abs(signedValue) : signedValue;
                total += weight * sample;
                weightSum += weight;
                frequency *= function.lacunarity;
                weight *= function.persistence;
            }
            heightMeters += function.amplitude_m * total / weightSum;
        }
        return heightMeters / metersPerUnit_;
    }

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

    const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions() const {
        return functions_;
    }
    const config::PlanetConfig::TerrainLod& lodSettings() const { return lod_; }

private:
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

    struct GridSample {
        glm::dvec3 position;
        double height;
    };

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
                grid[i].push_back({radial * (1.0 + height / radius_), height});
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
        const glm::dvec3& pa = a.position;
        const glm::dvec3& pb = b.position;
        const glm::dvec3& pc = c.position;
        const glm::dvec3 normal = glm::normalize(glm::cross(pb - pa, pc - pa));
        const unsigned int start = static_cast<unsigned int>(geometry.indices.size());
        for (const GridSample* sample : {&a, &b, &c}) {
            const double normalizedHeight = totalAmplitudeMeters_ == 0.0 ? 0.0 :
                sample->height / (totalAmplitudeMeters_ / metersPerUnit_);
            const float tint = static_cast<float>(0.72 + 0.45 * normalizedHeight);
            for (double component : {sample->position.x, sample->position.y,
                                     sample->position.z})
                geometry.vertices.push_back(static_cast<float>(component));
            for (double component : {normal.x, normal.y, normal.z})
                geometry.vertices.push_back(static_cast<float>(component));
            for (int channel = 0; channel < 3; ++channel)
                geometry.vertices.push_back(tint);
        }
        geometry.indices.insert(geometry.indices.end(), {start, start + 1, start + 2});
    }

    std::vector<config::PlanetConfig::SurfaceNoiseFunction> functions_;
    config::PlanetConfig::TerrainLod lod_;
    double radius_;
    double metersPerUnit_;
    double totalAmplitudeMeters_ = 0.0;
};

} // namespace rendering
