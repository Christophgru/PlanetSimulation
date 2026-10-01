#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <utility>

#include <glm/glm.hpp>
#include "config/ScenarioConfig.h"

namespace rendering {

// Positions and colors are interleaved as position, face normal, color factor.
// Faces own vertices, but shared corners receive the same sampled height tint.
// Landscape rendering uses per-fragment height; other bodies keep these tints.
struct TerrainGeometry {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::array<int, 3> zoneFaces{}; // compatibility summary: level 0, levels 1-6, level 7
    std::array<int, 8> lodFaces{}; // coarse to fine, one selected level per base face
    std::vector<int> faceZones; // levels 0-7, for LOD hysteresis
    // CPU-only displacement, shared/interpolated through shoreline refinement.
    // Applied once after tessellation; never added to the GPU vertex stride.
    std::vector<float> lodSinkMeters;
    int steepRefinedFaces = 0;
    int shorelineAddedTriangles = 0;
    int coarseNoiseSamples = 0;
    int fineNoiseSamples = 0;
    int triangleCount() const { return static_cast<int>(indices.size() / 3); }
};

class TerrainSurface {
public:
    TerrainSurface(const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions,
                   const config::PlanetConfig::TerrainLod& lod,
                   double radiusWorld, double metersPerWorldUnit,
                   const config::PlanetConfig::TerrainLandscape& landscape = {},
                   std::optional<double> waterLevelMeters = std::nullopt,
                   const config::PlanetConfig::TerrainMaterial& material = {});

    double heightAt(const glm::dvec3& radial) const;

    double regionPlainWeight(const glm::dvec3& radial) const;

    double regionCliffWeight(const glm::dvec3& radial) const;

    static glm::dvec3 landscapeColorFactors(double heightMeters, double slope,
                                             double waterLevelMeters,
                                             double beachWidthMeters,
                                             double maximumHeightMeters,
                                             const config::PlanetConfig::TerrainMaterial& material = {});

private:
    static double smoothstep(double low, double high, double value);

    double heightMeters(const glm::dvec3& direction, double detailWeight) const;

public:

    int lodLevel(double cameraDistanceWorld) const;

    TerrainGeometry buildGeometry(int edgeSegments) const;

    // Tessellate only the faces close to the camera. Every base-face edge is
    // sampled once at the finer of its two adjacent levels, then both faces
    // use those identical boundary samples. Concentric interior rings fill
    // unequal edge segment counts without T junctions or cracks.
    TerrainGeometry buildGeometryForEye(const glm::dvec3& eyeWorld,
                                        const glm::dvec3& planetCenter,
                                        const std::vector<int>* previousFaceZones = nullptr,
                                        double zoneHysteresisMeters = 0.0) const;

    const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions() const {
        return functions_;
    }
    const config::PlanetConfig::TerrainLod& lodSettings() const { return lod_; }

private:
    void refineShoreline(TerrainGeometry& geometry, const glm::dvec3& eyeBody) const;
    struct GridSample {
        glm::dvec3 position;
        double height;
        glm::dvec3 normal;
        glm::dvec3 color;
        double sinkMeters = 0.0;
    };
    struct SurfaceGradient {
        double slope;
        glm::dvec3 normal;
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
    static VertexKey vertexKey(const glm::dvec3& p);
    static EdgeKey edgeKey(const glm::dvec3& a, const glm::dvec3& b);
    static int ringCount(int segments);

    double maximumSlope(const BaseFace& face) const;

    static void collectBase(const glm::dvec3& a, const glm::dvec3& b,
                            const glm::dvec3& c, int remaining,
                            std::vector<BaseFace>& out);

    static std::vector<BaseFace> baseFaces();

    static std::uint32_t hash(int x, int y, int z, int seed);

    double valueNoise(const glm::dvec3& point, int seed) const;

    SurfaceGradient gradientAt(const glm::dvec3& radial, double heightMeters) const;

    glm::dvec3 colorAt(double heightWorld, double slope) const;

    GridSample makeGridSample(const glm::dvec3& radial, double heightWorld) const;

    void subdivideBase(const glm::dvec3& a, const glm::dvec3& b,
                       const glm::dvec3& c, int remaining,
                       int edgeSegments, TerrainGeometry& geometry) const;

    void emitGrid(const glm::dvec3& a, const glm::dvec3& b,
                  const glm::dvec3& c, int segments,
                  TerrainGeometry& geometry) const;

    void emitFace(const GridSample& a, const GridSample& b,
                  const GridSample& c, TerrainGeometry& geometry) const;

    std::vector<config::PlanetConfig::SurfaceNoiseFunction> functions_;
    config::PlanetConfig::TerrainLod lod_;
    config::PlanetConfig::TerrainLandscape landscape_;
    double radius_;
    double metersPerUnit_;
    double totalAmplitudeMeters_ = 0.0;
    std::optional<double> waterLevelMeters_;
    config::PlanetConfig::TerrainMaterial material_;
};

} // namespace rendering
