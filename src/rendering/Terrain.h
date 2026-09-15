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
// Each face owns its three vertices so its height tint is constant across it.
struct TerrainGeometry {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    int triangleCount() const { return static_cast<int>(indices.size() / 3); }
};

class TerrainSurface {
public:
    TerrainSurface(const config::PlanetConfig::SurfaceNoise& settings,
                   double radiusWorld, double metersPerWorldUnit)
        : settings_(settings), radius_(radiusWorld), metersPerUnit_(metersPerWorldUnit) {
        if (!std::isfinite(radius_) || radius_ <= 0.0 ||
            !std::isfinite(metersPerUnit_) || metersPerUnit_ <= 0.0 ||
            settings_.amplitude_m >= radius_ * metersPerUnit_) {
            throw std::invalid_argument("Invalid terrain radius or amplitude");
        }
    }

    double heightAt(const glm::dvec3& radial) const {
        const double length = glm::length(radial);
        if (!std::isfinite(length) || length <= 0.0) {
            throw std::invalid_argument("Terrain height needs a finite radial direction");
        }
        if (settings_.amplitude_m == 0.0) return 0.0;
        const glm::dvec3 direction = radial / length;
        double frequency = settings_.frequency;
        double weight = 1.0;
        double total = 0.0;
        double weightSum = 0.0;
        for (int octave = 0; octave < settings_.octaves; ++octave) {
            total += weight * (2.0 * valueNoise(direction * frequency) - 1.0);
            weightSum += weight;
            frequency *= settings_.lacunarity;
            weight *= settings_.persistence;
        }
        return settings_.amplitude_m / metersPerUnit_ * total / weightSum;
    }

    int lodLevel(double cameraDistanceWorld) const {
        if (!std::isfinite(cameraDistanceWorld) || cameraDistanceWorld < 0.0) {
            throw std::invalid_argument("Invalid camera distance for terrain LOD");
        }
        const double distanceDiameters = cameraDistanceWorld / (2.0 * radius_);
        if (distanceDiameters <= settings_.lod_near_diameters)
            return settings_.max_subdivisions;
        if (distanceDiameters >= settings_.lod_far_diameters)
            return settings_.base_subdivisions;
        const double portion = (settings_.lod_far_diameters - distanceDiameters) /
            (settings_.lod_far_diameters - settings_.lod_near_diameters);
        const int steps = settings_.max_subdivisions - settings_.base_subdivisions;
        return settings_.base_subdivisions +
               std::min(steps, static_cast<int>(portion * (steps + 1)));
    }

    TerrainGeometry buildGeometry(int subdivisions) const {
        if (subdivisions < 0 || subdivisions > settings_.max_subdivisions) {
            throw std::invalid_argument("Terrain subdivision exceeds configured cap");
        }
        TerrainGeometry geometry;
        int triangles = 20;
        for (int i = 0; i < subdivisions; ++i) triangles *= 4;
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
            subdivide(a, b, c, subdivisions, geometry);
        }
        return geometry;
    }

    const config::PlanetConfig::SurfaceNoise& settings() const { return settings_; }

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

    double valueNoise(const glm::dvec3& point) const {
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
                                              settings_.seed) / 4294967295.0;
                    result += value * (x ? f.x : 1.0 - f.x) *
                                      (y ? f.y : 1.0 - f.y) *
                                      (z ? f.z : 1.0 - f.z);
                }
            }
        }
        return result;
    }

    void subdivide(const glm::dvec3& a, const glm::dvec3& b,
                   const glm::dvec3& c, int remaining,
                   TerrainGeometry& geometry) const {
        if (remaining == 0) {
            emitFace(a, b, c, geometry);
            return;
        }
        const glm::dvec3 ab = glm::normalize(a + b);
        const glm::dvec3 bc = glm::normalize(b + c);
        const glm::dvec3 ca = glm::normalize(c + a);
        subdivide(a, ab, ca, remaining - 1, geometry);
        subdivide(b, bc, ab, remaining - 1, geometry);
        subdivide(c, ca, bc, remaining - 1, geometry);
        subdivide(ab, bc, ca, remaining - 1, geometry);
    }

    void emitFace(const glm::dvec3& a, const glm::dvec3& b,
                  const glm::dvec3& c, TerrainGeometry& geometry) const {
        const double ha = heightAt(a);
        const double hb = heightAt(b);
        const double hc = heightAt(c);
        const glm::dvec3 pa = a * (1.0 + ha / radius_);
        const glm::dvec3 pb = b * (1.0 + hb / radius_);
        const glm::dvec3 pc = c * (1.0 + hc / radius_);
        const glm::dvec3 normal = glm::normalize(glm::cross(pb - pa, pc - pa));
        const double normalizedHeight = settings_.amplitude_m == 0.0 ? 0.0 :
            (ha + hb + hc) / (3.0 * settings_.amplitude_m / metersPerUnit_);
        const float tint = static_cast<float>(0.72 + 0.45 * normalizedHeight);
        const unsigned int start = static_cast<unsigned int>(geometry.indices.size());
        for (const glm::dvec3& position : {pa, pb, pc}) {
            for (double component : {position.x, position.y, position.z})
                geometry.vertices.push_back(static_cast<float>(component));
            for (double component : {normal.x, normal.y, normal.z})
                geometry.vertices.push_back(static_cast<float>(component));
            for (int channel = 0; channel < 3; ++channel)
                geometry.vertices.push_back(tint);
        }
        geometry.indices.insert(geometry.indices.end(), {start, start + 1, start + 2});
    }

    config::PlanetConfig::SurfaceNoise settings_;
    double radius_;
    double metersPerUnit_;
};

} // namespace rendering
