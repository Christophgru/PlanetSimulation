#include "rendering/geometry/Terrain.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <tuple>

namespace rendering {

double TerrainSurface::heightAt(const glm::dvec3& radial) const {
    const double length = glm::length(radial);
    if (!std::isfinite(length) || length <= 0.0) {
        throw std::invalid_argument("Terrain height needs a finite radial direction");
    }
    const glm::dvec3 direction = radial / length;
    return heightMeters(direction, 1.0) / metersPerUnit_;
}

double TerrainSurface::regionPlainWeight(const glm::dvec3& radial) const {
    if (!landscape_.enabled) return 0.0;
    const double sample = valueNoise(glm::normalize(radial) * 2.7, landscape_.seed + 1);
    return 1.0 - smoothstep(landscape_.plain_threshold - 0.06,
                            landscape_.plain_threshold + 0.06, sample);
}

double TerrainSurface::regionCliffWeight(const glm::dvec3& radial) const {
    if (!landscape_.enabled) return 0.0;
    const double sample = valueNoise(glm::normalize(radial) * 2.4, landscape_.seed + 2);
    return smoothstep(landscape_.cliff_threshold - 0.06,
                      landscape_.cliff_threshold + 0.06, sample);
}

glm::dvec3 TerrainSurface::landscapeColorFactors(double heightMeters, double slope,
                                         double waterLevelMeters,
                                         double beachWidthMeters,
                                         double maximumHeightMeters,
                                         const config::PlanetConfig::TerrainMaterial& material) {
    const glm::dvec3 seabed(0.30, 0.40, 0.19);
    const glm::dvec3 beach(4.20, 1.90, 0.18);
    const glm::dvec3 grass(1.10, 1.30, 0.18);
    const glm::dvec3 snow(4.60, 2.30, 0.92);
    const double beachTop = waterLevelMeters + beachWidthMeters;
    const double beachFade = std::max(0.15, 0.15 * beachWidthMeters);
    const double usableRelief = std::max(1.0, maximumHeightMeters - waterLevelMeters);
    const double snowStart = std::max(beachTop + 1.0,
                                      waterLevelMeters + 0.25 * usableRelief);
    const double snowEnd = std::max(snowStart + 1.0,
                                    waterLevelMeters + 0.40 * usableRelief);

    const double aboveWater = smoothstep(
        waterLevelMeters - std::max(0.05, 0.05 * beachWidthMeters),
        waterLevelMeters + std::max(0.02, 0.02 * beachWidthMeters),
        heightMeters);
    const double beachWeight = aboveWater *
        (1.0 - smoothstep(beachTop, beachTop + beachFade, heightMeters));
    const double snowWeight = smoothstep(snowStart, snowEnd, heightMeters);
    glm::dvec3 land = glm::mix(grass, beach, beachWeight);
    land = glm::mix(land, snow, snowWeight);

    // Convert rise/run to the shader's 1-cos(angle) measure, keeping the
    // broad color darkening and fine gray-rock blend on the same range.
    const auto range = material.slopeMetricRange();
    const double steep = smoothstep(range[0], range[1],
        1.0 - 1.0 / std::sqrt(1.0 + slope * slope));
    land *= glm::mix(1.0, 0.50, steep);

    const double submerged = 1.0 - smoothstep(
        waterLevelMeters - std::max(0.5, 0.05 * beachWidthMeters),
        waterLevelMeters + std::max(0.02, 0.02 * beachWidthMeters),
        heightMeters);
    return glm::mix(land, seabed, submerged);
}

double TerrainSurface::smoothstep(double low, double high, double value) {
    const double t = std::clamp((value - low) / (high - low), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double TerrainSurface::heightMeters(const glm::dvec3& direction, double detailWeight) const {
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

int TerrainSurface::lodLevel(double cameraDistanceWorld) const {
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

TerrainGeometry TerrainSurface::buildGeometry(int edgeSegments) const {
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

TerrainSurface::VertexKey TerrainSurface::vertexKey(const glm::dvec3& p) {
    return {std::llround(p.x * 1e10), std::llround(p.y * 1e10),
            std::llround(p.z * 1e10)};
}

TerrainSurface::EdgeKey TerrainSurface::edgeKey(const glm::dvec3& a, const glm::dvec3& b) {
    const VertexKey ak = vertexKey(a), bk = vertexKey(b);
    return ak < bk ? EdgeKey{ak, bk} : EdgeKey{bk, ak};
}

int TerrainSurface::ringCount(int segments) { return std::max(1, (segments + 1) / 2); }

double TerrainSurface::maximumSlope(const BaseFace& face) const {
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

void TerrainSurface::collectBase(const glm::dvec3& a, const glm::dvec3& b,
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

std::vector<TerrainSurface::BaseFace> TerrainSurface::baseFaces() {
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

std::uint32_t TerrainSurface::hash(int x, int y, int z, int seed) {
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

double TerrainSurface::valueNoise(const glm::dvec3& point, int seed) const {
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

TerrainSurface::SurfaceGradient TerrainSurface::gradientAt(const glm::dvec3& radial, double heightMeters) const {
    const glm::dvec3 reference = std::abs(radial.z) < 0.8 ?
        glm::dvec3(0.0, 0.0, 1.0) : glm::dvec3(0.0, 1.0, 0.0);
    const glm::dvec3 tangentA = glm::normalize(glm::cross(reference, radial));
    const glm::dvec3 tangentB = glm::normalize(glm::cross(radial, tangentA));
    const double radiusMeters = radius_ * metersPerUnit_;
    const double angle = std::clamp(0.25 / radiusMeters, 1e-5, 0.01);
    const double distanceMeters = radiusMeters * angle;
    const glm::dvec3 sampleA = glm::normalize(
        std::cos(angle) * radial + std::sin(angle) * tangentA);
    const glm::dvec3 sampleB = glm::normalize(
        std::cos(angle) * radial + std::sin(angle) * tangentB);
    const double gradientA =
        (heightAt(sampleA) * metersPerUnit_ - heightMeters) / distanceMeters;
    const double gradientB =
        (heightAt(sampleB) * metersPerUnit_ - heightMeters) / distanceMeters;
    return {std::hypot(gradientA, gradientB),
            glm::normalize(radial - gradientA * tangentA - gradientB * tangentB)};
}

glm::dvec3 TerrainSurface::colorAt(double heightWorld, double slope) const {
    if (!landscape_.enabled) {
        const double normalizedHeight = totalAmplitudeMeters_ == 0.0 ? 0.0 :
            heightWorld / (totalAmplitudeMeters_ / metersPerUnit_);
        const double tint = 0.72 + 0.45 * normalizedHeight;
        return glm::dvec3(tint);
    }
    const double heightMeters = heightWorld * metersPerUnit_;
    return landscapeColorFactors(heightMeters, slope,
        waterLevelMeters_.value_or(0.0), 0.1,
        totalAmplitudeMeters_,
        material_);
}

TerrainSurface::GridSample TerrainSurface::makeGridSample(const glm::dvec3& radial, double heightWorld) const {
    const SurfaceGradient gradient = gradientAt(
        radial, heightWorld * metersPerUnit_);
    return {radial * (1.0 + heightWorld / radius_), heightWorld,
            gradient.normal, colorAt(heightWorld, gradient.slope)};
}

void TerrainSurface::subdivideBase(const glm::dvec3& a, const glm::dvec3& b,
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

void TerrainSurface::emitGrid(const glm::dvec3& a, const glm::dvec3& b,
              const glm::dvec3& c, int segments,
              TerrainGeometry& geometry) const {
    std::vector<std::vector<GridSample>> grid(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        for (int j = 0; i + j <= segments; ++j) {
            const glm::dvec3 radial = glm::normalize(
                static_cast<double>(segments - i - j) * a +
                static_cast<double>(i) * b + static_cast<double>(j) * c);
            const double height = heightAt(radial);
            grid[i].push_back(makeGridSample(radial, height));
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

void TerrainSurface::emitFace(const GridSample& a, const GridSample& b,
              const GridSample& c, TerrainGeometry& geometry) const {
    const GridSample* first = &a;
    const GridSample* second = &b;
    const GridSample* third = &c;
    const glm::dvec3& pa = a.position;
    const glm::dvec3& pb = b.position;
    const glm::dvec3& pc = c.position;
    if (glm::dot(glm::cross(pb - pa, pc - pa), pa + pb + pc) < 0.0)
        std::swap(second, third);
    const unsigned int start = static_cast<unsigned int>(geometry.indices.size());
    for (const GridSample* sample : {first, second, third}) {
        for (double component : {sample->position.x, sample->position.y,
                                 sample->position.z})
            geometry.vertices.push_back(static_cast<float>(component));
        for (double component : {sample->normal.x, sample->normal.y,
                                 sample->normal.z})
            geometry.vertices.push_back(static_cast<float>(component));
        for (double component : {sample->color.x, sample->color.y, sample->color.z})
            geometry.vertices.push_back(static_cast<float>(component));
        geometry.lodSinkMeters.push_back(static_cast<float>(sample->sinkMeters));
    }
    geometry.indices.insert(geometry.indices.end(), {start, start + 1, start + 2});
}

TerrainSurface::TerrainSurface(const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions,
               const config::PlanetConfig::TerrainLod& lod,
               double radiusWorld, double metersPerWorldUnit,
               const config::PlanetConfig::TerrainLandscape& landscape,
               std::optional<double> waterLevelMeters,
               const config::PlanetConfig::TerrainMaterial& material)
    : functions_(functions), lod_(lod), landscape_(landscape), radius_(radiusWorld),
      metersPerUnit_(metersPerWorldUnit), waterLevelMeters_(waterLevelMeters), material_(material) {
    lod_.validate();
    landscape_.validate();
    material_.validate();
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
    if (waterLevelMeters_ && !std::isfinite(*waterLevelMeters_))
        throw std::invalid_argument("Invalid terrain water level");
}
} // namespace rendering
