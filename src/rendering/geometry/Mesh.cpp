#include "rendering/geometry/Mesh.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/geometry/compute/TerrainCompute.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <limits>
#include <algorithm>

namespace {
void validateCpuMorph(const rendering::TerrainGeometry& g) {
    if(!g.morphData) return;
    const auto& d=*g.morphData;const auto n=d.samples();
    if(!n || d.targets.size()!=18*n || d.corners.size()!=g.indices.size() ||
       g.indices.empty() || g.indices.size()%3 || g.vertices.size()!=9*g.indices.size() ||
       (!g.grassCellKeys.empty() && g.grassCellKeys.size()*3!=g.indices.size()) ||
       g.indices.size()>std::size_t(std::numeric_limits<GLsizei>::max()) ||
       d.generation!=g.generation || g.generation.backend!=rendering::TerrainBackend::Cpu ||
       !g.generation.field || !g.generation.topology || g.generation.topologyVersion!=4 ||
       g.generation.fieldVersion!=rendering::PlanetField::version || g.uniqueSamples!=n ||
       !std::isfinite(d.metersPerRadius) || d.metersPerRadius<=0)
        throw std::invalid_argument("Invalid retained CPU terrain source");
    if(d.phase) {
        rendering::validateTerrainMorphFootprint(*d.phase);
        if(d.phase->metersPerRadius!=d.metersPerRadius)
            throw std::invalid_argument("Retained CPU terrain phase scale mismatch");
    }
    for(std::size_t i=0;i<n;++i) {
        const auto radial=d.radial(i),fine=glm::dvec3(d.fine(i)),parent=d.parent(i),original=glm::dvec3(d.original(i));
        for(int j=0;j<3;++j) if(!std::isfinite(radial[j]) || !std::isfinite(fine[j]) ||
            !std::isfinite(parent[j]) || !std::isfinite(original[j]))
            throw std::invalid_argument("Retained CPU terrain requires finite targets");
        if(std::abs(glm::dot(radial,radial)-1)>1e-12)
            throw std::invalid_argument("Retained CPU terrain requires unit directions");
    }
    for(std::size_t c=0;c<g.indices.size();++c) {
        if(g.indices[c]!=c || d.corners[c]>=n)
            throw std::invalid_argument("Retained CPU terrain requires matching expanded corners");
        const auto sample=d.corners[c];auto expected=d.original(sample);
        if(d.phase) {
            const auto fine=glm::dvec3(d.fine(sample));
            const double w=rendering::terrainMorphWeight(d.radial(sample),*d.phase);
            expected=w==0?glm::vec3(fine):glm::vec3(fine+(d.parent(sample)-fine)*w);
        }
        for(int j=0;j<3;++j) {
            const double value=g.vertices[9*c+j];
            // The retained fine endpoint is float, while the CPU helper can
            // keep intermediates at double precision before its final cast.
            const double rounding=d.phase?4*std::numeric_limits<float>::epsilon()*std::max(1.,std::abs(double(expected[j]))):0;
            if(!std::isfinite(value) || std::abs(value-expected[j])>rounding)
                throw std::invalid_argument("Retained CPU terrain pose does not match its source");
        }
    }
}
}

void Mesh::addVertex(float x, float y, float z, float nx, float ny, float nz) {
    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(z);
    vertices.push_back(nx);
    vertices.push_back(ny);
    vertices.push_back(nz);
}

void Mesh::addTriangle(unsigned int idx0, unsigned int idx1, unsigned int idx2) {
    indices.push_back(idx0);
    indices.push_back(idx1);
    indices.push_back(idx2);
}

void Mesh::buildSphereGeometry(int segments) {
    if (segments < 3) throw std::invalid_argument("Sphere needs at least 3 segments");
    terrainStats={};
    vertices.clear();
    indices.clear();
    grassCellKeys.clear();cellKeyOffset=0;
    hasVertexColors = false;

    // Generate sphere with proper triangle indices
    for (int lat = 0; lat <= segments; lat++) {
        for (int lon = 0; lon <= segments; lon++) {
            float theta = M_PI * 2.0f * lon / segments;
            float phi = M_PI * lat / segments;

            float x = cos(theta) * sin(phi);
            float y = cos(phi);
            float z = sin(theta) * sin(phi);

            // Normal is same as position for unit sphere
            float nx = x;
            float ny = y;
            float nz = z;

            addVertex(x, y, z, nx, ny, nz);
        }
    }

    // Generate indices for triangle list
    for (int lat = 0; lat < segments; lat++) {
        for (int lon = 0; lon < segments; lon++) {
            unsigned int first = (lat * (segments + 1) + lon);
            unsigned int second = first + segments + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
}

void Mesh::generateSphere(int segments) {
    buildSphereGeometry(segments);
    upload();
}

void Mesh::buildCubeGeometry() {
    terrainStats={};
    vertices.clear();
    indices.clear();
    grassCellKeys.clear();cellKeyOffset=0;
    hasVertexColors = false;
    constexpr float corners[8][3] = {
        {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
        { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f},
        { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}
    };
    for (const auto& corner : corners)
        addVertex(corner[0], corner[1], corner[2], 0.0f, 0.0f, 0.0f);
    constexpr unsigned int triangles[36] = {
        0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1,
        5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4,
        3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4
    };
    indices.assign(std::begin(triangles), std::end(triangles));
}

void Mesh::generateCube() {
    buildCubeGeometry();
    upload();
}

void Mesh::loadTerrain(rendering::TerrainGeometry geometry) {
    validateCpuMorph(geometry);
    auto source=std::move(geometry.morphData);
    struct SourceBuffers {
        std::array<GLuint,2> ids{};
        ~SourceBuffers() {if(ids[0] || ids[1]) glDeleteBuffers(2,ids.data());}
    } buffers;
    if(source) {
        GLint texels=0,previous=0;glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&texels);
        if(texels<=0 || source->targets.size()>std::size_t(texels) || source->corners.size()>std::size_t(texels))
            throw std::runtime_error("Retained CPU terrain exceeds texture buffer capacity");
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);
        try {
            glGenBuffers(2,buffers.ids.data());
            const std::array<const std::vector<std::uint32_t>*,2> data{&source->targets,&source->corners};
            for(int i=0;i<2;++i) {
                glBindBuffer(GL_ARRAY_BUFFER,buffers.ids[i]);
                glBufferData(GL_ARRAY_BUFFER,data[i]->size()*sizeof(std::uint32_t),data[i]->data(),GL_STATIC_DRAW);
            }
            if(!buffers.ids[0] || !buffers.ids[1] || glGetError()!=GL_NO_ERROR)
                throw std::runtime_error("Retained CPU terrain buffer allocation failed");
        } catch(...) {glBindBuffer(GL_ARRAY_BUFFER,previous);throw;}
        glBindBuffer(GL_ARRAY_BUFFER,previous);
    }
    terrainStats=static_cast<const rendering::TerrainBuildStats&>(geometry);
    vertices = std::move(geometry.vertices);
    indices = std::move(geometry.indices);
    grassCellKeys=std::move(geometry.grassCellKeys);
    hasVertexColors = true;
    upload();
    if(source) {
        morphTargets=std::exchange(buffers.ids[0],0);morphCorners=std::exchange(buffers.ids[1],0);
        morphSamples=source->samples();morphData=std::move(source);
        // CPU poses are baked: their mesh revision identifies the positions.
        // The shared live state is reserved for an actual deformation adapter.
    }
}

void Mesh::upload() {
    contacts.reset();
    releaseMorphTargets();
    residentTerrain=false;indexCount=indices.size();
    ++revision;
    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    if (ebo == 0) glGenBuffers(1, &ebo);

    const int stride = hasVertexColors ? 9 : 6;

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), 
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    if(!grassCellKeys.empty() && grassCellKeys.size()*3!=indices.size())
        throw std::invalid_argument("Terrain cell keys require matching triangle indices");
    cellKeyOffset=grassCellKeys.empty()?0:indices.size();
    const auto indexBytes=indices.size()*sizeof(unsigned int),cellBytes=grassCellKeys.size()*16;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,indexBytes+cellBytes,nullptr,GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,indexBytes,indices.data());
    if(cellBytes) glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,indexBytes,cellBytes,grassCellKeys.data());

    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), 
                         (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    if (hasVertexColors) {
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
                              (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    } else {
        glDisableVertexAttribArray(2);
    }

    glBindVertexArray(0);
}

void Mesh::loadComputedTerrain(rendering::TerrainGeometry geometry,
    rendering::TerrainComputeBuffers& buffers, bool keepCpuMirror) {
    auto key=geometry.generation;key.backend=rendering::TerrainBackend::Compute;
    if(!buffers.complete || !buffers.vao || !buffers.vbo || !buffers.ebo || key!=buffers.stats.generation ||
       (keepCpuMirror && (geometry.indices.size()!=buffers.stats.gpuCorners || geometry.vertices.size()!=9*buffers.stats.gpuCorners)) ||
       (!keepCpuMirror && (!geometry.indices.empty() || !geometry.vertices.empty())) ||
       (buffers.retainedMorph && (!buffers.scratch[1] || !buffers.scratch[2] || !buffers.stats.uniqueSamples)) ||
       (buffers.contacts && buffers.contacts->generation()!=buffers.stats.generation))
        throw std::invalid_argument("Incomplete or incompatible computed terrain generation");
    auto phase=buffers.retainedMorph?std::make_shared<rendering::TerrainMorphState>():nullptr;
    // Caller publishes only after the whole land/water pair has completed.
    destroy();
    vao=std::exchange(buffers.vao,0);vbo=std::exchange(buffers.vbo,0);ebo=std::exchange(buffers.ebo,0);
    if(buffers.retainedMorph) {
        morphTargets=std::exchange(buffers.scratch[1],0);morphCorners=std::exchange(buffers.scratch[2],0);
        morphSamples=buffers.stats.uniqueSamples;
        morphState=std::move(phase);
    }
    terrainStats=std::move(buffers.stats);
    contacts=std::move(buffers.contacts);
    terrainStats.evaluationQueries=geometry.evaluationQueries;
    vertices=std::move(geometry.vertices);indices=std::move(geometry.indices);
    residentTerrain=!keepCpuMirror;indexCount=terrainStats.gpuCorners;
    grassCellKeys=std::move(geometry.grassCellKeys);
    cellKeyOffset=terrainStats.gpuCellKeys?indexCount:0;
    hasVertexColors=true;++revision;
}

void Mesh::draw() const {
    if (vao == 0 || vbo == 0 || ebo == 0) {
        std::cerr << "Mesh not initialized\n";
        return;
    }

    glBindVertexArray(vao);
    if (!hasVertexColors) glVertexAttrib3f(2, 1.0f, 1.0f, 1.0f);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void Mesh::releaseMorphTargets() {
    if(morphTargets || morphCorners) {
        const GLuint buffers[]{morphTargets,morphCorners};glDeleteBuffers(2,buffers);
    }
    morphTargets=morphCorners=0;morphSamples=0;
    morphState.reset();
    morphData.reset();
}
std::uint64_t Mesh::deformationRevision() const {return morphState?morphState->revision():0;}
std::uint64_t Mesh::cpuMorphBytes() const {
    return morphData?(morphData->targets.capacity()+morphData->corners.capacity())*sizeof(std::uint32_t):0;
}
std::uint64_t Mesh::gpuMorphBytes() const {return morphTargets?72*morphSamples+4*indexCount:0;}
rendering::TerrainMorphSource Mesh::morphSource() const {
    return morphTargets || morphCorners || morphSamples?
        rendering::TerrainMorphSource{morphTargets,morphCorners,morphSamples,terrainStats.generation}:rendering::TerrainMorphSource{};
}
void Mesh::destroy() {
    contacts.reset();
    releaseMorphTargets();
    if (vao != 0) glDeleteVertexArrays(1, &vao);
    if (vbo != 0) glDeleteBuffers(1, &vbo);
    if (ebo != 0) glDeleteBuffers(1, &ebo);
    vao = vbo = ebo = 0;indexCount=cellKeyOffset=0;residentTerrain=false;
}

void Mesh::swap(Mesh& other) noexcept {
    static_assert(std::is_nothrow_swappable_v<rendering::TerrainBuildStats>);
    using std::swap;
    swap(vao,other.vao);swap(vbo,other.vbo);swap(ebo,other.ebo);
    swap(morphTargets,other.morphTargets);swap(morphCorners,other.morphCorners);swap(morphSamples,other.morphSamples);
    morphState.swap(other.morphState);
    morphData.swap(other.morphData);
    vertices.swap(other.vertices);indices.swap(other.indices);
    grassCellKeys.swap(other.grassCellKeys);swap(cellKeyOffset,other.cellKeyOffset);
    swap(hasVertexColors,other.hasVertexColors);swap(residentTerrain,other.residentTerrain);
    swap(indexCount,other.indexCount);swap(revision,other.revision);
    swap(terrainStats,other.terrainStats);contacts.swap(other.contacts);
}
