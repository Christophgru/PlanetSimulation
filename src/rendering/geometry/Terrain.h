#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <utility>

#include <glm/glm.hpp>
#include "config/ScenarioConfig.h"
#include "rendering/geometry/terrain/TerrainTopology.h"

namespace rendering {

// Positions and colors are interleaved as position, face normal, color factor.
// Faces own vertices, but shared corners receive the same sampled height tint.
// Landscape rendering uses per-fragment height; other bodies keep these tints.
struct TerrainGeometry : TerrainBuildStats {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<float> lodSinkMeters;
    int triangleCount() const { return static_cast<int>(indices.size()/3); }
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
        return field_.functions();
    }
    const config::PlanetConfig::TerrainLod& lodSettings() const { return lod_; }
    std::uint32_t topologyVersion() const { return lod_.local_detail_radius_m>0 ? 3 : lod_.relief_sinking ? 2 : 1; }
    const PlanetField& field() const { return field_; }
    TerrainTopology buildTopology(int edgeSegments) const;
    TerrainTopology buildTopologyForEye(const glm::dvec3& eyeWorld,const glm::dvec3& center,
        const std::vector<int>* previousFaceZones=nullptr,double hysteresisMeters=0.0) const;
    TerrainGeometry evaluateTopology(const TerrainTopology& topology) const;

private:
    static double smoothstep(double low, double high, double value);
    void refineShoreline(TerrainTopology& geometry, const glm::dvec3& eyeBody, TerrainQueryCache& queries, int budget) const;
    void refineSurfaceError(TerrainTopology& geometry, const glm::dvec3& eyeBody) const;
    struct GridSample {
        glm::dvec3 radial;
        glm::dvec3 position;
        double height;
        double sinkMeters = 0.0;
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

    double maximumSlope(const BaseFace& face, TerrainQueryCache& queries) const;

    static void collectBase(const glm::dvec3& a, const glm::dvec3& b,
                            const glm::dvec3& c, int remaining,
                            std::vector<BaseFace>& out);

    static std::vector<BaseFace> baseFaces();

    GridSample makeGridSample(const glm::dvec3& radial, double heightWorld) const;

    void subdivideBase(const glm::dvec3& a, const glm::dvec3& b,
                       const glm::dvec3& c, int remaining,
                       int edgeSegments, TerrainTopology& geometry, TerrainQueryCache& queries) const;

    void emitGrid(const glm::dvec3& a, const glm::dvec3& b,
                  const glm::dvec3& c, int segments,
                  TerrainTopology& geometry, TerrainQueryCache& queries) const;

    void emitFace(const GridSample& a, const GridSample& b,
                  const GridSample& c, TerrainTopology& geometry) const;

    PlanetField field_;
    config::PlanetConfig::TerrainLod lod_;
    double radius_;
    double metersPerUnit_;
    double totalAmplitudeMeters_ = 0.0;
    std::optional<double> waterLevelMeters_;
};

} // namespace rendering
