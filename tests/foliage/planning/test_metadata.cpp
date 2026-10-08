#include "rendering/foliage/planning/GrassMetadata.h"
#include "rendering/foliage/procedural/GrassPlan.h"
#include "rendering/geometry/compute/TerrainCompute.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
using namespace rendering;
namespace {
config::PlanetConfig planet() {
    config::ScenarioConfig scene(config::Config::load("configs/scenarios/solar_system.json"));
    auto p=scene.planets[0];p.radius=1;p.foliage.enabled=true;
    p.foliage.max_blades=25000000;p.foliage.max_candidates_per_triangle=1;
    p.terrain_lod.max_triangle_budget=10000;p.terrain_lod.max_edge_segments=8;
    p.terrain_lod.steep_edge_segments=8;p.terrain_lod.medium_edge_segments=4;
    return p;
}
TerrainSurface surface(const config::PlanetConfig& p) {
    return TerrainSurface(p.surface_noise,p.terrain_lod,p.radius,1000,p.terrain_landscape,
        p.water.enabled?std::optional<double>(p.water.level_m):std::nullopt,p.terrain_material);
}
void compare(const config::PlanetConfig& p,const glm::dvec3& eye,bool forceChunks=false) {
    auto s=surface(p);const auto topology=s.buildTopologyForEye(eye,{0,0,0});
    TerrainCompute terrain;auto mesh=terrain.generate(s.field(),topology);mesh->waitForCapture();
    const auto vertices=mesh->readVertices();const auto indices=mesh->readIndices();
    const auto cpu=planGrass(vertices,indices,p,1000,eye);
    auto limits=GrassMetadataLimits::query();ASSERT_TRUE(limits.unavailable.empty()) << limits.unavailable;
    if(forceChunks) limits.groups=1;
    GrassMetadataCompute compute(limits);
    auto gpu=compute.generate(mesh->vbo,mesh->ebo,mesh->stats,p,1000,eye);
    EXPECT_THROW(gpu->readForValidation(),std::logic_error);
    EXPECT_EQ(gpu->diagnosticReadBytes,0u);gpu->waitForCapture();ASSERT_TRUE(gpu->poll());
    const auto metadata=gpu->readForValidation();ASSERT_EQ(metadata.size(),topology.triangleCount());
    const auto parameters=grassMetadataParameters(p,1000,eye,metadata.size());
    std::map<std::uint32_t,const ProceduralGrassPatch*> patches;
    for(const auto& patch:cpu.patches) patches.emplace(patch.triangle,&patch);
    double maximumRelativeAreaError=0;
    for(std::size_t t=0;t<metadata.size();++t) {
        const auto& m=metadata[t];EXPECT_EQ(m.identity[0],t);EXPECT_EQ(m.identity[2],0u);EXPECT_EQ(m.identity[3],0u);
        const auto found=patches.find(t);ASSERT_EQ(m.identity[1]!=0,found!=patches.end()) << "triangle " << t;
        for(double value:m.centerReach) ASSERT_TRUE(std::isfinite(value));
        for(double value:m.areaDistance) ASSERT_TRUE(std::isfinite(value));
        if(found==patches.end()) {EXPECT_EQ(m.areaDistance[0],0);continue;}
        const auto& patch=*found->second;
        const glm::dvec3 a(patch.a),b(patch.b),c(patch.c),center=(a+b+c)/3.0;
        const double reach=std::max({glm::length(a-center),glm::length(b-center),glm::length(c-center)});
        const double distance=glm::length(center-eye)*1000;
        const double minimumDistance=std::max(0.0,distance-reach*1000-parameters.ranges[1]);
        const double area=glm::length(glm::cross(b-a,c-a))*.5*1000*1000;
        const double weighted=area*std::exp(-minimumDistance*minimumDistance/parameters.ranges[2]);
        maximumRelativeAreaError=std::max(maximumRelativeAreaError,std::abs(m.areaDistance[0]-weighted)/weighted);
        for(int axis=0;axis<3;++axis) EXPECT_NEAR(m.centerReach[axis],center[axis],1e-12);
        EXPECT_NEAR(m.centerReach[3],reach,1e-12);EXPECT_NEAR(m.areaDistance[1],distance,1e-8);
        EXPECT_NEAR(m.areaDistance[0],weighted,std::max(1e-14,weighted*2e-10));
    }
    EXPECT_EQ(gpu->generation,mesh->stats.generation);EXPECT_EQ(gpu->planningEye,eye);
    EXPECT_EQ(gpu->workingBytes,160+64*metadata.size());EXPECT_EQ(gpu->inputBytes,160+8*gpu->dispatches);
    EXPECT_EQ(gpu->diagnosticReadBytes,64*metadata.size());
    if(forceChunks) EXPECT_EQ(gpu->dispatches,(metadata.size()+63)/64);
    std::cout << "GRASS_METADATA triangles=" << metadata.size() << " eligible=" << cpu.patches.size()
        << " uploaded=" << gpu->inputBytes << " resident=" << gpu->workingBytes
        << " dispatches=" << gpu->dispatches << " relative_area_error=" << maximumRelativeAreaError << '\n';
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
}
TEST(GrassMetadata, MatchesCpuEligibilityAndWeightsAtPolesWithWaterSnowSlopesAndGaussianTails) {
    auto p=planet();
    compare(p,{1.012,0,0},true);
    compare(p,{0,0,1.012});compare(p,{0,0,-1.012});
    p.foliage.gaussian_sigma_fraction=.05;compare(p,{1.002,0,0});
    p.foliage.gaussian_sigma_fraction=1;p.water.enabled=false;compare(p,{1.002,0,0});
    p.terrain_landscape.enabled=false;p.surface_noise.clear();p.color={.2,.8,.2};
    compare(p,{1.002,0,0});p.color={.8,.2,.2};compare(p,{1.002,0,0});
    p.foliage.enabled=false;compare(p,{1.002,0,0});p.foliage.enabled=true;compare(p,{3,0,0});
}
TEST(GrassMetadata, DriverPackingMatchesHostDescriptorsAndParameters) {
    auto p=planet();auto s=surface(p);const auto topology=s.buildTopology(1);
    TerrainCompute terrain;auto mesh=terrain.generate(s.field(),topology);mesh->waitForCapture();
    GrassMetadataCompute compute;auto metadata=compute.generate(mesh->vbo,mesh->ebo,mesh->stats,p,1000,{1.002,0,0});
    metadata->waitForCapture();
    for(const auto [name,expected]:std::array<std::pair<const char*,int>,8>{{
        {"eyeScale",0},{"ranges",32},{"biome",64},{"colorGreen",96},{"flags",128},
        {"descriptors[0].centerReach",0},{"descriptors[0].areaDistance",32},{"descriptors[0].identity",48}}}) {
        const auto index=glGetProgramResourceIndex(compute.program(),GL_BUFFER_VARIABLE,name);ASSERT_NE(index,GL_INVALID_INDEX) << name;
        GLint actual=0;const GLenum offset=GL_OFFSET;
        glGetProgramResourceiv(compute.program(),GL_BUFFER_VARIABLE,index,1,&offset,1,nullptr,&actual);
        EXPECT_EQ(actual,expected) << name;
        if(std::string(name).starts_with("descriptors")) {
            const GLenum stride=GL_TOP_LEVEL_ARRAY_STRIDE;
            glGetProgramResourceiv(compute.program(),GL_BUFFER_VARIABLE,index,1,&stride,1,nullptr,&actual);EXPECT_EQ(actual,64);
        }
    }
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GrassMetadata, LimitsTruncatedInputsAndInvalidGenerationsRejectWithoutDisturbingGlRanges) {
    auto p=planet();auto s=surface(p);const auto topology=s.buildTopology(1);
    TerrainCompute terrain;auto mesh=terrain.generate(s.field(),topology);mesh->waitForCapture();
    GLuint sentinel=0;glGenBuffers(1,&sentinel);glBindBuffer(GL_SHADER_STORAGE_BUFFER,sentinel);
    glBufferData(GL_SHADER_STORAGE_BUFFER,1024,nullptr,GL_STATIC_DRAW);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER,0,sentinel,256,512);
    GrassMetadataCompute compute;const glm::dvec3 eye(1.002,0,0);
    auto assertState=[&] {
        GLint binding=0;GLint64 start=0,size=0;
        glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,0,&binding);
        glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,0,&start);glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,0,&size);
        EXPECT_EQ(binding,sentinel);EXPECT_EQ(start,256);EXPECT_EQ(size,512);
        glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&binding);EXPECT_EQ(binding,sentinel);
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    };
    auto metadata=compute.generate(mesh->vbo,mesh->ebo,mesh->stats,p,1000,eye);assertState();metadata->waitForCapture();
    EXPECT_THROW(compute.generate(sentinel,mesh->ebo,mesh->stats,p,1000,eye),std::invalid_argument);assertState();
    auto wrong=mesh->stats;wrong.generation.backend=TerrainBackend::Cpu;
    EXPECT_THROW(compute.generate(mesh->vbo,mesh->ebo,wrong,p,1000,eye),std::invalid_argument);
    wrong=mesh->stats;wrong.gpuCorners++;
    EXPECT_THROW(compute.generate(mesh->vbo,mesh->ebo,wrong,p,1000,eye),std::invalid_argument);
    wrong=mesh->stats;wrong.generation.topologyVersion=3; // Versions 1 and 2 are supported.
    EXPECT_THROW(compute.generate(mesh->vbo,mesh->ebo,wrong,p,1000,eye),std::invalid_argument);
    auto limits=GrassMetadataLimits::query();limits.blockBytes=159;GrassMetadataCompute tiny(limits);
    EXPECT_THROW(tiny.generate(mesh->vbo,mesh->ebo,mesh->stats,p,1000,eye),std::runtime_error);
    limits=GrassMetadataLimits::query();limits.groups=0;GrassMetadataCompute zero(limits);
    EXPECT_THROW(zero.generate(mesh->vbo,mesh->ebo,mesh->stats,p,1000,eye),std::runtime_error);
    EXPECT_THROW(compute.generate(mesh->vbo,mesh->ebo,mesh->stats,p,1000,{0,0,0}),std::invalid_argument);
    assertState();glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,0);glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);glDeleteBuffers(1,&sentinel);
}
TEST(GrassMetadata, ProductionResidentBuffersDoNotNeedTheCpuMirrorAndCachedPreparationTransfersNothing) {
    auto p=planet();p.terrain_lod.max_triangle_budget=100000;
    compare(p,{1.012,0,0});
    auto s=surface(p);const auto topology=s.buildTopologyForEye({1.002,0,0},{0,0,0});
    TerrainCompute terrain;auto gpu=terrain.generate(s.field(),topology);gpu->waitForCapture();
    Mesh mesh;mesh.loadComputedTerrain(s.evaluateTopology(topology),*gpu);
    ProceduralGrass grass;
    const auto first=grass.prepare(0,mesh,p,1000,{1.002,0,0});const auto stats=grass.stats(0);
    EXPECT_GT(stats.metadataBytes,0u);EXPECT_GT(stats.metadataInputBytes,0u);EXPECT_EQ(stats.metadataReadBytes,0u);
    EXPECT_EQ(first.uploadedBytes,stats.patchBytes+stats.metadataInputBytes);
    EXPECT_EQ(grass.prepare(0,mesh,p,1000,{1.002,0,0}).uploadedBytes,0u);
    // Metadata itself consumes only resident buffers; poisoned CPU values
    // cannot alter it. CPU allocation remains separate until T3b2.
    mesh.vertices.clear();mesh.indices.clear();GrassMetadataCompute compute;
    auto metadata=compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,{1.002,0,0});
    metadata->waitForCapture();EXPECT_EQ(metadata->diagnosticReadBytes,0u);
    EXPECT_EQ(metadata->triangles,topology.triangleCount());
    p.foliage.enabled=false;grass.prepare(0,mesh,p,1000,{1.002,0,0});EXPECT_EQ(grass.stats(0).metadataBytes,0u);
    grass.clear();mesh.destroy();EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
