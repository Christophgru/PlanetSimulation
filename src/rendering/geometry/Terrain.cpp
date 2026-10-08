#include "rendering/geometry/Terrain.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <tuple>

namespace rendering {

double TerrainSurface::heightAt(const glm::dvec3& radial) const { return field_.heightAt(radial); }
double TerrainSurface::regionPlainWeight(const glm::dvec3& radial) const { return field_.regionPlainWeight(radial); }
double TerrainSurface::regionCliffWeight(const glm::dvec3& radial) const { return field_.regionCliffWeight(radial); }
double TerrainSurface::smoothstep(double a,double b,double x) { return PlanetField::smoothstep(a,b,x); }
glm::dvec3 TerrainSurface::landscapeColorFactors(double h,double slope,double water,double beach,double maximum,
    const config::PlanetConfig::TerrainMaterial& material) {
    return PlanetField::landscapeColorFactors(h,slope,water,beach,maximum,material);
}
TerrainSurface::GridSample TerrainSurface::makeGridSample(const glm::dvec3& radial,double height) const {
    return {radial,radial*(1.0+height/radius_),height,0};
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
    return evaluateTopology(buildTopology(edgeSegments));
}

TerrainTopology TerrainSurface::buildTopology(int edgeSegments) const {
    if (edgeSegments < 1 || edgeSegments > lod_.max_edge_segments) {
        throw std::invalid_argument("Terrain edge segments exceed configured cap");
    }
    TerrainTopology geometry;
    TerrainQueryCache queries(field_);
    const int triangles = 320 * edgeSegments * edgeSegments;
    geometry.samples.reserve(static_cast<std::size_t>(triangles)*3);
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
        subdivideBase(a, b, c, 2, edgeSegments, geometry, queries);
    }
    geometry.planningQueries=queries.stats();
    geometry.canonicalize(field_.fingerprint());
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

double TerrainSurface::maximumSlope(const BaseFace& face, TerrainQueryCache& queries) const {
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
        heights.push_back(queries.heightAt(sample) * metersPerUnit_);
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

void TerrainSurface::subdivideBase(const glm::dvec3& a, const glm::dvec3& b,
                   const glm::dvec3& c, int remaining,
                   int edgeSegments, TerrainTopology& geometry, TerrainQueryCache& queries) const {
    if (remaining == 0) {
        emitGrid(a, b, c, edgeSegments, geometry, queries);
        return;
    }
    const glm::dvec3 ab = glm::normalize(a + b);
    const glm::dvec3 bc = glm::normalize(b + c);
    const glm::dvec3 ca = glm::normalize(c + a);
    subdivideBase(a, ab, ca, remaining - 1, edgeSegments, geometry, queries);
    subdivideBase(b, bc, ab, remaining - 1, edgeSegments, geometry, queries);
    subdivideBase(c, ca, bc, remaining - 1, edgeSegments, geometry, queries);
    subdivideBase(ab, bc, ca, remaining - 1, edgeSegments, geometry, queries);
}

void TerrainSurface::emitGrid(const glm::dvec3& a, const glm::dvec3& b,
              const glm::dvec3& c, int segments,
              TerrainTopology& geometry, TerrainQueryCache& queries) const {
    std::vector<std::vector<GridSample>> grid(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        for (int j = 0; i + j <= segments; ++j) {
            const glm::dvec3 radial = glm::normalize(
                static_cast<double>(segments - i - j) * a +
                static_cast<double>(i) * b + static_cast<double>(j) * c);
            const double height = queries.heightAt(radial);
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

void TerrainSurface::emitFace(const GridSample& a,const GridSample& b,
    const GridSample& c,TerrainTopology& geometry) const {
    const GridSample* first=&a;const GridSample* second=&b;const GridSample* third=&c;
    if(glm::dot(glm::cross(b.position-a.position,c.position-a.position),a.position+b.position+c.position)<0)
        std::swap(second,third);
    const auto start=static_cast<std::uint32_t>(geometry.samples.size());
    for(const auto* sample:{first,second,third}) {
        geometry.planningPositions.push_back(glm::vec3(sample->position));
        geometry.samples.push_back({{sample->radial.x,sample->radial.y,sample->radial.z},
            static_cast<double>(static_cast<float>(sample->sinkMeters))});
    }
    geometry.indices.insert(geometry.indices.end(),{start,start+1,start+2});
}

TerrainGeometry TerrainSurface::evaluateTopology(const TerrainTopology& topology) const {
    CpuTrace::Scope scope("TerrainSurface::evaluateTopology");
    topology.validate();
    if(topology.generation.field!=field_.fingerprint())
        throw std::invalid_argument("Terrain topology belongs to a different planet field");
    TerrainQueryCache queries(field_,8192,topology.surfacePolicy);
    std::vector<std::array<float,9>> values;values.reserve(topology.samples.size());
    for(const auto& s:topology.samples) {
        const glm::dvec3 radial(s.radial[0],s.radial[1],s.radial[2]);
        const auto v=field_.sample(radial,queries.heightAt(radial),&queries,topology.surfacePolicy);
        std::array<float,9> packed{};
        for(int j=0;j<3;++j) {packed[j]=static_cast<float>(v.position[j]);
            packed[j+3]=static_cast<float>(v.normal[j]);packed[j+6]=static_cast<float>(v.color[j]);}
        // Match legacy sinking after the first float vertex conversion.
        if(s.sinkMeters!=0) {
            const glm::dvec3 p(packed[0],packed[1],packed[2]);
            const auto sunk=p-glm::normalize(p)*(s.sinkMeters/(radius_*metersPerUnit_));
            for(int j=0;j<3;++j) packed[j]=static_cast<float>(sunk[j]);
        }
        values.push_back(packed);
    }
    TerrainGeometry geometry;
    static_cast<TerrainBuildStats&>(geometry)=static_cast<const TerrainBuildStats&>(topology);
    geometry.evaluationQueries=queries.stats();
    geometry.vertices.reserve(topology.indices.size()*9);
    geometry.indices.reserve(topology.indices.size());geometry.lodSinkMeters.reserve(topology.indices.size());
    for(auto index:topology.indices) {
        const auto& v=values[index];geometry.vertices.insert(geometry.vertices.end(),v.begin(),v.end());
        geometry.indices.push_back(static_cast<unsigned>(geometry.indices.size()));
        geometry.lodSinkMeters.push_back(static_cast<float>(topology.samples[index].sinkMeters));
    }
    return geometry;
}

TerrainSurface::TerrainSurface(const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions,
    const config::PlanetConfig::TerrainLod& lod,double radiusWorld,double metersPerWorldUnit,
    const config::PlanetConfig::TerrainLandscape& landscape,std::optional<double> water,
    const config::PlanetConfig::TerrainMaterial& material)
    : field_(functions,radiusWorld,metersPerWorldUnit,landscape,water,material),
      lod_(lod),radius_(radiusWorld),metersPerUnit_(metersPerWorldUnit),
      totalAmplitudeMeters_(field_.maximumReliefMeters()),waterLevelMeters_(water) { lod_.validate(); }
} // namespace rendering
