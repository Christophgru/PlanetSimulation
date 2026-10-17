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
    EXPECT_EQ(gpu->workingBytes,160+128*metadata.size());EXPECT_EQ(gpu->inputBytes,164+8*gpu->dispatches);
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
TEST(GrassMetadata, ImmutableGeometryReuseMatchesFreshCameraWeightsAndInvalidatesChangedInputs) {
    auto p=planet();auto s=surface(p);const auto topology=s.buildTopology(1);
    TerrainCompute terrain;auto mesh=terrain.generate(s.field(),topology);mesh->waitForCapture();
    GrassMetadataCompute compute;
    auto first=compute.generate(mesh->vbo,mesh->ebo,mesh->stats,p,1000,{1.002,0,0});first->waitForCapture();
    const auto original=first->readForValidation();const auto buffer=first->geometry->buffer;
    const glm::dvec3 eye(1.01,.025,0);
    auto reused=compute.generate(mesh->vbo,mesh->ebo,mesh->stats,p,1000,eye,{},first->geometry);
    auto fresh=compute.generate(mesh->vbo,mesh->ebo,mesh->stats,p,1000,eye);
    reused->waitForCapture();fresh->waitForCapture();
    ASSERT_TRUE(reused->geometryReused);EXPECT_EQ(reused->geometry,first->geometry);
    EXPECT_EQ(first->readForValidation().size(),original.size());
    const auto compare=[](const auto& a,const auto& b) {
        ASSERT_EQ(a.size(),b.size());
        for(std::size_t i=0;i<a.size();++i) {
            EXPECT_EQ(a[i].centerReach,b[i].centerReach);EXPECT_EQ(a[i].areaDistance,b[i].areaDistance);
            EXPECT_EQ(a[i].identity,b[i].identity);
        }
    };
    compare(reused->readForValidation(),fresh->readForValidation());
    compare(first->readForValidation(),original); // Refresh never mutates live descriptors.
    auto changed=p;changed.foliage.water_clearance_m+=.1;
    auto invalid=compute.generate(mesh->vbo,mesh->ebo,mesh->stats,changed,1000,eye,{},first->geometry);
    EXPECT_FALSE(invalid->geometryReused);invalid->waitForCapture();
    auto generation=mesh->stats;++generation.generation.topology;
    auto replaced=compute.generate(mesh->vbo,mesh->ebo,generation,p,1000,eye,{},first->geometry);
    EXPECT_FALSE(replaced->geometryReused);replaced->waitForCapture();
    EXPECT_FALSE(first->geometry->matches(mesh->ebo,mesh->vbo,mesh->stats,first->inputs));
    first.reset();EXPECT_TRUE(glIsBuffer(buffer));reused.reset();EXPECT_FALSE(glIsBuffer(buffer));
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
    wrong=mesh->stats;wrong.generation.topologyVersion=99; // Versions 1 through 4 are supported.
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
TEST(GrassMetadata, ImmutableMorphSourceMatchesInitialGeometryAcrossLiveDeformationAndCacheReuse) {
    auto p=planet();p.surface_noise.clear();p.terrain_landscape.enabled=p.water.enabled=false;
    p.color={1,1,1};p.foliage.draw_distance_m=5;p.foliage.quad_distance_m=2;
    p.foliage.max_blades=100000;p.foliage.density_per_m2=2;
    auto& l=p.terrain_lod;l.local_detail_streaming=true;l.local_detail_radius_m=1.5;
    l.local_edge_m=.05;l.local_error_m=.01;l.local_transition_m=.5;l.relief_sinking=true;
    l.geometric_error_m=.05;l.max_triangle_budget=100000;l.shoreline_edge_m=0;
    l.near_surface_distance_m=300;l.mid_surface_distance_m=1000;
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m=.01;noise.wavelength_m=.25;noise.octaves=1;noise.seed=-71;p.surface_noise={noise};
    auto s=surface(p);const auto radial=glm::normalize(glm::dvec3(.7,-.2,1));
    const auto eye=radial*1.002,east=glm::normalize(glm::cross(radial,glm::dvec3(0,0,1)));
    const auto topology=s.buildTopologyForEye(eye,{});ASSERT_FALSE(topology.localDetailLimited);
    TerrainCompute terrain;auto gpu=terrain.generate(s.field(),topology,false,TerrainMorphStorage::Retained);
    gpu->waitForCapture();
    struct Owner {
        Mesh mesh;GLuint sentinel=0;
        ~Owner() {
            for(int i:{5,6}) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,0);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);glUseProgram(0);
            glDeleteBuffers(1,&sentinel);mesh.destroy();
        }
    } owner;
    TerrainGeometry metadata;static_cast<TerrainBuildStats&>(metadata)=topology;
    owner.mesh.loadComputedTerrain(std::move(metadata),*gpu,false);gpu.reset();auto& mesh=owner.mesh;
    const TerrainMorphSource source{mesh.morphTargets,mesh.morphCorners,mesh.morphSamples,mesh.terrainStats.generation};
    GLint alignment=0;glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,&alignment);
    glGenBuffers(1,&owner.sentinel);glBindBuffer(GL_SHADER_STORAGE_BUFFER,owner.sentinel);
    glBufferData(GL_SHADER_STORAGE_BUFFER,alignment+64,nullptr,GL_STATIC_DRAW);
    for(int i:{5,6}) glBindBufferRange(GL_SHADER_STORAGE_BUFFER,i,owner.sentinel,alignment,64);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    const auto assertState=[&] {
        GLint storage=0;glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&storage);EXPECT_EQ(storage,0);
        for(int i:{5,6}) {
            GLint buffer=0;GLint64 start=0,size=0;
            glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,i,&buffer);
            glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,i,&start);glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,i,&size);
            EXPECT_EQ(buffer,owner.sentinel);EXPECT_EQ(start,alignment);EXPECT_EQ(size,64);
        }
    };
    GrassMetadataCompute compute;
    auto original=compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye);
    original->waitForCapture();const auto expected=original->readForValidation();
    auto initial=compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye,{},original->geometry,source);
    initial->waitForCapture();EXPECT_FALSE(initial->geometryReused);assertState();
    const auto compare=[&](const auto& actual) {
        ASSERT_EQ(actual.size(),expected.size());
        for(std::size_t i=0;i<actual.size();++i) {
            ASSERT_EQ(actual[i].centerReach,expected[i].centerReach) << i;
            ASSERT_EQ(actual[i].areaDistance,expected[i].areaDistance) << i;
            ASSERT_EQ(actual[i].identity,expected[i].identity) << i;
        }
    };
    compare(initial->readForValidation());
    auto envelope=compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye,{},nullptr,source,GrassMetadataMode::MorphEnvelope);
    envelope->waitForCapture();assertState();
    const auto readBuffer=[](GLuint buffer,auto& values) {
        GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
        glBindBuffer(GL_COPY_READ_BUFFER,buffer);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        glGetBufferSubData(GL_COPY_READ_BUFFER,0,values.size()*sizeof(values[0]),values.data());
        glBindBuffer(GL_COPY_READ_BUFFER,previous);
    };
    std::vector<GrassTriangleMetadata> bounds(topology.triangleCount());
    readBuffer(envelope->geometry->buffer,bounds); // Unweighted bounds, including distant triangles.
    std::vector<float> liveVertices(9*topology.indices.size());
    std::uint64_t bounded=0;
    std::size_t changed=0;
    for(double offset:{0.,.15,.30,0.}) {
        SCOPED_TRACE(offset);terrain.deform(mesh,{glm::normalize(radial+east*(offset/1000)),1000,.5,.5});
        readBuffer(mesh.vbo,liveVertices);
        for(std::size_t t=0;t<bounds.size();++t) {
            const auto& bound=bounds[t];
            const glm::dvec3 center(bound.centerReach[0],bound.centerReach[1],bound.centerReach[2]);
            std::array<glm::dvec3,3> positions;
            for(int c=0;c<3;++c) {
                // Compute emits one float corner per EBO index, in face order.
                const auto i=9*(3*t+c);positions[c]={liveVertices[i],liveVertices[i+1],liveVertices[i+2]};
                ASSERT_LE(glm::length(positions[c]-center),bound.centerReach[3]) << t;
            }
            const auto area=.5e6*glm::length(glm::cross(positions[1]-positions[0],positions[2]-positions[0]));
            ASSERT_LE(area,bound.areaDistance[0]) << t;++bounded;
        }
        auto fresh=compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye,{},nullptr,source);
        fresh->waitForCapture();EXPECT_FALSE(fresh->geometryReused);compare(fresh->readForValidation());assertState();
        auto reused=compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye,{},initial->geometry,source);
        reused->waitForCapture();EXPECT_TRUE(reused->geometryReused);compare(reused->readForValidation());assertState();
        EXPECT_EQ(fresh->workingBytes,original->workingBytes);EXPECT_EQ(fresh->inputBytes,original->inputBytes+4);
        auto live=compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye,{},initial->geometry);
        live->waitForCapture();EXPECT_FALSE(live->geometryReused);
        const auto actual=live->readForValidation();
        for(std::size_t i=0;i<actual.size();++i) changed+=actual[i].centerReach!=expected[i].centerReach;
    }
    EXPECT_GT(changed,0u);
    auto invalid=source;invalid.generation.topology++;
    EXPECT_THROW(compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye,{},nullptr,invalid),std::invalid_argument);
    invalid=source;invalid.samples++;
    EXPECT_THROW(compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye,{},nullptr,invalid),std::invalid_argument);
    invalid=source;invalid.targets=0;
    EXPECT_THROW(compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye,{},nullptr,invalid),std::invalid_argument);
    invalid=source;invalid.targets=owner.sentinel;
    EXPECT_THROW(compute.generate(mesh.vbo,mesh.ebo,mesh.terrainStats,p,1000,eye,{},nullptr,invalid),std::invalid_argument);
    assertState();EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    std::cout << "IMMUTABLE_MORPH_METADATA triangles=" << expected.size() << " changed_live_triangle_queries=" << changed
        << " bounded_live_triangle_queries=" << bounded << " extra_gpu_storage_bytes=0 extra_uniform_bytes=4\n";
}
