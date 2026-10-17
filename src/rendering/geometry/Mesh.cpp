#include "rendering/geometry/Mesh.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/geometry/compute/TerrainCompute.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <type_traits>

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
    terrainStats=static_cast<const rendering::TerrainBuildStats&>(geometry);
    vertices = std::move(geometry.vertices);
    indices = std::move(geometry.indices);
    grassCellKeys=std::move(geometry.grassCellKeys);
    hasVertexColors = true;
    upload();
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
}
std::uint64_t Mesh::deformationRevision() const {return morphState?morphState->revision():0;}
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
    vertices.swap(other.vertices);indices.swap(other.indices);
    grassCellKeys.swap(other.grassCellKeys);swap(cellKeyOffset,other.cellKeyOffset);
    swap(hasVertexColors,other.hasVertexColors);swap(residentTerrain,other.residentTerrain);
    swap(indexCount,other.indexCount);swap(revision,other.revision);
    swap(terrainStats,other.terrainStats);contacts.swap(other.contacts);
}
