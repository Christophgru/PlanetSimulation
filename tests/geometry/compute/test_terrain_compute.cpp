#include "rendering/geometry/compute/TerrainCompute.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "rendering/character/SurfaceContact.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstring>
#include <iostream>
#include <chrono>
#include <bit>
using namespace rendering;
namespace {
config::PlanetConfig::TerrainLod lod() {
    config::PlanetConfig::TerrainLod l;l.max_edge_segments=l.steep_edge_segments=8;
    l.medium_edge_segments=4;l.max_triangle_budget=10000;l.shoreline_edge_m=8;l.shoreline_distance_m=300;return l;
}
TerrainTopology probes(const PlanetField& field) {
    TerrainTopology t;
    for(auto p:{glm::dvec3(1,0,0),glm::dvec3(-1,0,0),glm::dvec3(0,0,1),glm::dvec3(0,0,-1),
        glm::dvec3(.6,0,.8-1e-13),glm::dvec3(.6,0,.8+1e-13),glm::dvec3(-.7,.2,-.5),
        glm::dvec3(-1,1e-13,0),glm::dvec3(-1,-1e-13,0)}) {
        p=glm::normalize(p);t.samples.push_back({{p.x,p.y,p.z},.2});
    }
    for(unsigned i=0;i<99;++i) t.indices.push_back(i%t.samples.size());
    t.canonicalize(field.fingerprint());return t;
}
void parity(const TerrainSurface& surface,const TerrainTopology& t,TerrainCompute& compute,bool closed=false) {
    const auto begin=std::chrono::steady_clock::now();
    const auto cpu=surface.evaluateTopology(t);
    const auto sampled=std::chrono::steady_clock::now();
    auto gpu=compute.generate(surface.field(),t,true);
    const auto submitted=std::chrono::steady_clock::now();
    EXPECT_THROW(gpu->readVertices(),std::logic_error);
    gpu->waitForCapture();ASSERT_TRUE(gpu->poll());
    const auto finished=std::chrono::steady_clock::now();
    const auto values=gpu->readVertices();const auto heights=gpu->readHeights();
    // GPU-generated planet-fixed cell identities match the independent CPU
    // topology oracle, including negative directions and cyclic corner order.
    std::vector<std::array<std::uint32_t,4>> keys(t.triangleCount());
    glBindBuffer(GL_COPY_READ_BUFFER,gpu->ebo);
    glGetBufferSubData(GL_COPY_READ_BUFFER,t.indices.size()*4,keys.size()*16,keys.data());
    glBindBuffer(GL_COPY_READ_BUFFER,0);EXPECT_EQ(keys,cpu.grassCellKeys);
    const auto read=std::chrono::steady_clock::now();
    ASSERT_EQ(values.size(),cpu.vertices.size());ASSERT_EQ(heights.size(),t.samples.size());
    for(float value:values) ASSERT_TRUE(std::isfinite(value));
    double heightError=0,positionError=0,normalError=0,colorError=0;
    std::vector<std::size_t> firstUse(t.samples.size(),std::numeric_limits<std::size_t>::max());
    for(std::size_t i=0;i<heights.size();++i) {
        ASSERT_TRUE(std::isfinite(heights[i]));const auto& r=t.samples[i].radial;
        heightError=std::max(heightError,std::abs(heights[i]-surface.field().heightAt({r[0],r[1],r[2]},t.surfacePolicy)*surface.field().metersPerUnit()));
    }
    for(std::size_t i=0;i<values.size();i+=9) {
        const auto v=[&](const std::vector<float>& array,int offset) {return glm::dvec3(array[i+offset],array[i+offset+1],array[i+offset+2]);};
        ASSERT_TRUE(std::isfinite(values[i]));
        positionError=std::max(positionError,glm::length(v(values,0)-v(cpu.vertices,0))*surface.field().radiusWorld()*surface.field().metersPerUnit());
        normalError=std::max(normalError,std::acos(std::clamp(glm::dot(glm::normalize(v(values,3)),glm::normalize(v(cpu.vertices,3))),-1.0,1.0)));
        colorError=std::max(colorError,glm::length(v(values,6)-v(cpu.vertices,6)));
        // Every occurrence of a canonical sample copies identical GPU endpoint bits.
        auto& first=firstUse[t.indices[i/9]];if(first==std::numeric_limits<std::size_t>::max()) first=i/9;
        EXPECT_EQ(std::memcmp(values.data()+i,values.data()+first*9,36),0);
    }
    if(closed) {
        std::map<std::pair<std::uint32_t,std::uint32_t>,int> edgeUses;
        for(std::size_t i=0;i<t.indices.size();i+=3) {
            std::array<glm::dvec3,3> p;
            for(int j=0;j<3;++j) {
                const auto a=t.indices[i+j],b=t.indices[i+(j+1)%3];
                ++edgeUses[std::minmax(a,b)];
                p[j]={values[(i+j)*9],values[(i+j)*9+1],values[(i+j)*9+2]};
            }
            EXPECT_GE(glm::dot(glm::cross(p[1]-p[0],p[2]-p[0]),p[0]+p[1]+p[2]),-1e-18);
        }
        for(const auto& [edge,uses]:edgeUses) EXPECT_EQ(uses,2);
    }
    EXPECT_LE(heightError,.0001);EXPECT_LE(positionError,.002);
    EXPECT_LE(normalError,.1*std::acos(-1.0)/180);EXPECT_LE(colorError,2e-5);
    EXPECT_EQ(gpu->readIndices(),cpu.indices);
    EXPECT_EQ(gpu->stats.generation.backend,TerrainBackend::Compute);
    EXPECT_GT(gpu->stats.gpuDispatches,1u);EXPECT_GE(gpu->stats.gpuMilliseconds,0);
    std::cout << "PARITY triangles=" << t.triangleCount() << " samples=" << t.samples.size()
        << " height_m=" << heightError << " position_m=" << positionError << " normal_rad=" << normalError
        << " color=" << colorError << " uploaded=" << gpu->stats.gpuInputBytes
        << " peak=" << gpu->stats.gpuWorkingBytes << " dispatches=" << gpu->stats.gpuDispatches
        << " gpu_ms=" << gpu->stats.gpuMilliseconds
        << " cpu_bulk_ms=" << std::chrono::duration<double,std::milli>(sampled-begin).count()
        << " submit_ms=" << std::chrono::duration<double,std::milli>(submitted-sampled).count()
        << " capture_wait_ms=" << std::chrono::duration<double,std::milli>(finished-submitted).count()
        << " validation_read_ms=" << std::chrono::duration<double,std::milli>(read-finished).count() << '\n';
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
}
TEST(TerrainCompute, DoubleFieldsMatchCpuAtPolesLatticeBoundariesAndExtremeSeeds) {
    auto limits=TerrainComputeLimits::query();ASSERT_TRUE(limits.unavailable.empty()) << limits.unavailable;
    limits.groups=1;TerrainCompute compute(limits); // Force partial/chunked dispatches.
    for(int kind=0;kind<6;++kind) {
        config::PlanetConfig::SurfaceNoiseFunction n;n.amplitude_m=8;n.seed=kind%2?-71:std::numeric_limits<int>::max();
        n.type=kind%2?"ridged_fbm":"value_fbm";n.frequency=kind>2?64:8;n.octaves=6;n.lacunarity=4;
        config::PlanetConfig::TerrainLandscape l;l.enabled=kind>=2;l.seed=std::numeric_limits<int>::max();
        l.continent_amplitude_m=10;l.cliff_amplitude_m=20;l.ridge_smoothing=kind%2?.03:0;
        const TerrainSurface s(kind==0?std::vector<config::PlanetConfig::SurfaceNoiseFunction>{}:std::vector{n},lod(),kind==5?.27:1,1000,l,0);
        parity(s,probes(s.field()),compute);
    }
    const TerrainSurface si({},lod(),1000,1);parity(si,probes(si.field()),compute);
}
TEST(TerrainCompute, ShorelineMixedLodAndSinkingRetainSharedEndpointsAndTriangleOrder) {
    TerrainCompute compute;const TerrainSurface s({},lod(),1,1000,{},0);
    const auto t=s.buildTopologyForEye({1.002,0,0},{0,0,0});
    EXPECT_GT(t.shorelineAddedTriangles,0);EXPECT_EQ(t.triangleCount(),10000);
    parity(s,t,compute,true);
}
TEST(TerrainCompute, ReliefSinkingNoiseAndSparseContactsMatchGpuAtMixedLevels) {
    auto settings=lod();settings.relief_sinking=true;settings.near_surface_distance_m=50;
    settings.geometric_error_m=.05;
    settings.mid_surface_distance_m=1600;
    config::PlanetConfig::SurfaceNoiseFunction noise;noise.amplitude_m=8;noise.frequency=64;noise.octaves=6;
    const TerrainSurface surface({noise},settings,1,1000,{},0);
    const auto topology=surface.buildTopologyForEye({1.002,0,0},{});
    TerrainCompute compute;parity(surface,topology,compute,true);
    auto gpu=compute.generate(surface.field(),topology);gpu->waitForCapture();
    const auto vertices=gpu->readVertices();const auto indices=gpu->readIndices();
    SurfaceContact rendered,sparse;rendered.bind(vertices,indices,1,1000);
    sparse.bind(std::make_shared<SparseTerrainContacts>(surface.field(),topology),1);
    const GroundQuery missing=[](const auto&) -> GroundContact {throw std::logic_error("Missing contact plane");};
    for(double angle:{0.0,.05,.1,.5,1.0,2.0,3.14}) {
        const glm::dvec3 direction(std::cos(angle),std::sin(angle),0);
        EXPECT_LE(glm::length(rendered.sample(direction,missing).position-sparse.sample(direction,missing).position),.002);
    }
}
TEST(TerrainCompute, PhysicalMicroReliefAndCentimeterTopologyMatchSparseContacts) {
    auto settings=lod();settings.shoreline_edge_m=0;settings.relief_sinking=true;
    settings.geometric_error_m=.05;settings.local_detail_radius_m=.25;
    settings.local_edge_m=.05;settings.local_transition_m=.5;
    config::PlanetConfig::SurfaceNoiseFunction n;n.amplitude_m=.01;n.wavelength_m=.25;
    n.octaves=1;n.seed=std::numeric_limits<int>::max();
    const TerrainSurface surface({n},settings,1,1000);
    const auto topology=surface.buildTopologyForEye({1.002,0,0},{});
    ASSERT_EQ(topology.generation.topologyVersion,3u);ASSERT_FALSE(topology.localDetailLimited);
    TerrainCompute compute;parity(surface,topology,compute,true);
    auto gpu=compute.generate(surface.field(),topology);gpu->waitForCapture();
    const auto vertices=gpu->readVertices();const auto indices=gpu->readIndices();
    SurfaceContact rendered,sparse;rendered.bind(vertices,indices,1,1000);
    sparse.bind(std::make_shared<SparseTerrainContacts>(surface.field(),topology),1);
    const GroundQuery missing=[](const auto&) -> GroundContact {throw std::logic_error("Missing contact plane");};
    for(int i=-10;i<=10;++i) {
        const auto r=glm::normalize(glm::dvec3(1,i*.017/1000,.013/1000));
        const auto a=rendered.sample(r,missing),b=sparse.sample(r,missing);
        EXPECT_LE(glm::length(a.position-b.position),.0001);
        EXPECT_LE(std::abs(glm::length(a.position)-1000-surface.field().heightAt(r)*1000),.01);
    }
}
TEST(TerrainCompute, DriverPackingMatchesTheFieldContractAndScalarOutputStride) {
    TerrainCompute compute;const TerrainSurface s({},lod(),1,1000);
    auto gpu=compute.generate(s.field(),probes(s.field()),true);gpu->waitForCapture();
    for(const auto [name,expected]:std::array<std::pair<const char*,int>,7>{{
        {"scale",0},{"gradient",128},{"flags",160},{"seeds",176},
        {"noises[0].amplitude",192},{"noises[0].settings",224},{"noises[0].padding",240}}}) {
        const auto index=glGetProgramResourceIndex(compute.program(),GL_BUFFER_VARIABLE,name);
        ASSERT_NE(index,GL_INVALID_INDEX) << name;
        GLint offset=0;const GLenum property=GL_OFFSET;
        glGetProgramResourceiv(compute.program(),GL_BUFFER_VARIABLE,index,1,&property,1,nullptr,&offset);
        EXPECT_EQ(offset,expected) << name;
    }
    for(const auto [name,stride]:std::array<std::pair<const char*,int>,3>{{{"inputs[0]",32},{"uniqueValues[0]",4},{"vertices[0]",4}}}) {
        const auto index=glGetProgramResourceIndex(compute.program(),GL_BUFFER_VARIABLE,name);
        ASSERT_NE(index,GL_INVALID_INDEX) << name;GLint actual=0;const GLenum property=GL_ARRAY_STRIDE;
        glGetProgramResourceiv(compute.program(),GL_BUFFER_VARIABLE,index,1,&property,1,nullptr,&actual);
        EXPECT_EQ(actual,stride) << name;
    }
}
TEST(TerrainCompute, ProductionFieldMatchesCpuAndReducesTheCountedInputPayload) {
    const config::ScenarioConfig scene(config::Config::load("configs/scenarios/solar_system.json"));
    const auto& p=scene.planets[0];
    const TerrainSurface s(p.surface_noise,p.terrain_lod,p.radius,scene.metersPerWorldUnit(),
        p.terrain_landscape,p.water.level_m,p.terrain_material);
    const auto topology=s.buildTopologyForEye({1.012,0,0},{0,0,0});
    EXPECT_EQ(topology.triangleCount(),100000);TerrainCompute compute;parity(s,topology,compute,true);
    auto gpu=compute.generate(s.field(),topology);gpu->waitForCapture();
    EXPECT_LT(gpu->stats.gpuInputBytes,topology.triangleCount()*120*.25);
    EXPECT_EQ(gpu->stats.gpuInputBytes,topology.topologyInputBytes+20+16*gpu->stats.gpuDispatches+(topology.surfacePolicy.enabled()?0:sizeof(TerrainSurfacePolicy)));
}
// F5 canonical receipt: topology is built once outside both timed intervals.
// Numerical/geometry parity is independently checked by the production test
// above. Timing is reported, never asserted as a machine-dependent unit gate.
TEST(TerrainCompute, Canonical100kTransferAndBulkCpuWorkReceipt) {
    const config::ScenarioConfig scene(config::Config::load("configs/scenarios/solar_system.json"));
    const auto& p=scene.planets[0];
    const TerrainSurface surface(p.surface_noise,p.terrain_lod,p.radius,scene.metersPerWorldUnit(),
        p.terrain_landscape,p.water.level_m,p.terrain_material);
    const auto topology=surface.buildTopologyForEye({1.012,0,0},{0,0,0});
    ASSERT_EQ(topology.triangleCount(),100000);
    TerrainCompute compute;
    for(int trial=-1;trial<3;++trial) {
        TerrainGeometry cpu;
        std::unique_ptr<TerrainComputeBuffers> gpu;
        double cpuMs=0,submitMs=0,waitMs=0;
        const auto cpuWork=[&] {
            const auto start=std::chrono::steady_clock::now();
            cpu=surface.evaluateTopology(topology);
            cpuMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        };
        const auto gpuWork=[&] {
            const auto start=std::chrono::steady_clock::now();
            gpu=compute.generate(surface.field(),topology);
            const auto submitted=std::chrono::steady_clock::now();
            gpu->waitForCapture();
            submitMs=std::chrono::duration<double,std::milli>(submitted-start).count();
            waitMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-submitted).count();
        };
        if(trial%2) {cpuWork();gpuWork();} else {gpuWork();cpuWork();}
        const auto cpuBytes=cpu.vertices.size()*sizeof(float)+cpu.indices.size()*sizeof(unsigned);
        EXPECT_EQ(cpuBytes,12000000u);
        EXPECT_LE(gpu->stats.gpuInputBytes,cpuBytes/4);
        EXPECT_GT(cpu.evaluationQueries.evaluations,0u);
        EXPECT_EQ(gpu->stats.evaluationQueries.requests,0u);
        EXPECT_EQ(gpu->stats.evaluationQueries.evaluations,0u);
        EXPECT_EQ(gpu->stats.uniqueSamples,cpu.uniqueSamples);
        if(trial>=0) std::cout << "CANONICAL trial=" << trial << " triangles=" << topology.triangleCount()
            << " cpu_input_bytes=" << cpuBytes << " gpu_input_bytes=" << gpu->stats.gpuInputBytes
            << " cpu_bulk_evaluations=" << cpu.evaluationQueries.evaluations
            << " gpu_bulk_cpu_evaluations=" << gpu->stats.evaluationQueries.evaluations
            << " cpu_bulk_ms=" << cpuMs << " gpu_cpu_submit_ms=" << submitMs
            << " gpu_ms=" << gpu->stats.gpuMilliseconds << " capture_wait_ms=" << waitMs << '\n';
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
}
TEST(TerrainCompute, FailedGenerationCannotReplaceAnActiveMeshAndNoCpuVerticesAreUploaded) {
    TerrainCompute compute;const TerrainSurface s({},lod(),1,1000);const auto t=probes(s.field());
    auto gpu=compute.generate(s.field(),t);auto geometry=s.evaluateTopology(t);Mesh mesh;
    EXPECT_THROW(mesh.loadComputedTerrain(geometry,*gpu),std::invalid_argument);EXPECT_EQ(mesh.vbo,0);
    gpu->waitForCapture();const auto actual=gpu->readVertices();
    geometry.vertices[0]+=1; // Sentinel would expose an accidental shaped-vertex upload.
    mesh.loadComputedTerrain(std::move(geometry),*gpu);EXPECT_EQ(gpu->vbo,0);
    const auto revision=mesh.revision;const auto vbo=mesh.vbo;
    auto tiny=TerrainComputeLimits::query();tiny.blockBytes=703;TerrainCompute restricted(tiny);
    EXPECT_THROW(restricted.generate(s.field(),t),std::runtime_error);
    EXPECT_EQ(mesh.revision,revision);EXPECT_EQ(mesh.vbo,vbo);
    glBindBuffer(GL_COPY_READ_BUFFER,mesh.vbo);std::vector<float> read(actual.size());
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,read.size()*4,read.data());EXPECT_EQ(read,actual);
    EXPECT_NE(read[0],mesh.vertices[0]);
    const TerrainSurface other({},lod(),1,2000);EXPECT_THROW(compute.generate(other.field(),t),std::invalid_argument);
    EXPECT_EQ(mesh.terrainStats.generation.backend,TerrainBackend::Compute);
    EXPECT_GT(mesh.terrainStats.evaluationQueries.requests,0u);
    mesh.destroy();EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainCompute, ImmutableSnapshotAndMutableInputsProduceIdenticalBuffersAndRejectStaleFields) {
    TerrainCompute compute;const TerrainSurface surface({},lod(),1,1000);
    auto raw=probes(surface.field());const TerrainTopologySnapshot snapshot(raw);
    auto mutableGpu=compute.generate(surface.field(),raw,true);
    auto frozenGpu=compute.generate(surface.field(),snapshot,true);
    mutableGpu->waitForCapture();frozenGpu->waitForCapture();
    EXPECT_EQ(mutableGpu->readVertices(),frozenGpu->readVertices());
    EXPECT_EQ(mutableGpu->readIndices(),frozenGpu->readIndices());
    EXPECT_EQ(mutableGpu->readHeights(),frozenGpu->readHeights());
    raw.samples[0].sinkMeters+=1;
    EXPECT_THROW(compute.generate(surface.field(),raw),std::invalid_argument);
    const TerrainSurface changed({},lod(),1,2000);
    EXPECT_THROW(compute.generate(changed.field(),snapshot),std::invalid_argument);
    EXPECT_THROW(compute.generate(surface.field(),TerrainTopologySnapshot{}),std::invalid_argument);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainCompute, RetainedTargetsDeformWithCpuParityAndPreserveAttributesAndOwnership) {
    auto l=lod();l.local_detail_streaming=true;l.local_detail_radius_m=1.5;
    l.local_edge_m=.05;l.local_error_m=.01;l.local_transition_m=.5;
    l.relief_sinking=true;l.geometric_error_m=.05;l.max_triangle_budget=100000;
    l.near_surface_distance_m=300;l.mid_surface_distance_m=1000;l.shoreline_edge_m=0;
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m=.01;noise.wavelength_m=.25;noise.octaves=1;noise.seed=-71;
    const TerrainSurface surface({noise},l,1,1000);
    const glm::dvec3 eye(-.21005697617230545,.909856724515421,-.36613409503800903);
    const auto topology=surface.buildTopologyForEye(eye,{});
    ASSERT_FALSE(topology.localDetailLimited);
    TerrainCompute compute;auto ordinary=compute.generate(surface.field(),topology);
    auto retained=compute.generate(surface.field(),TerrainTopologySnapshot(topology),false,TerrainMorphStorage::Retained);
    ordinary->waitForCapture();retained->waitForCapture();
    EXPECT_EQ(retained->stats.gpuWorkingBytes,ordinary->stats.gpuWorkingBytes);
    EXPECT_GT(retained->stats.gpuDispatches,ordinary->stats.gpuDispatches);
    const auto initial=retained->readVertices();const auto outputIndices=retained->readIndices();
    const auto ordinaryVertices=ordinary->readVertices();
    ASSERT_EQ(initial.size(),ordinaryVertices.size());
    double initialDifference=0;
    for(std::size_t i=0;i<initial.size();++i) {
        ASSERT_TRUE(std::isfinite(initial[i]));
        initialDifference=std::max(initialDifference,std::abs(double(initial[i])-ordinaryVertices[i]));
    }
    EXPECT_LE(initialDifference,.000002);
    EXPECT_EQ(outputIndices,ordinary->readIndices());
    struct Owner {Mesh mesh;~Owner(){mesh.destroy();}} owner;
    auto& mesh=owner.mesh;
    TerrainGeometry metadata;static_cast<TerrainBuildStats&>(metadata)=topology;
    retained->contacts=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
    const auto targets=retained->scratch[1],corners=retained->scratch[2];
    mesh.loadComputedTerrain(std::move(metadata),*retained,false);
    ASSERT_TRUE(mesh.morphState);EXPECT_EQ(mesh.morphState->revision(),0u);
    SurfaceContact sharedContact;sharedContact.bind(mesh.contacts,mesh.revision,mesh.morphState);
    std::weak_ptr<TerrainMorphState> phaseLifetime=mesh.morphState;
    EXPECT_EQ(retained->scratch[1],0u);EXPECT_EQ(retained->scratch[2],0u);
    retained.reset();ordinary.reset(); // Preparation scratch is gone; retained views still own their buffers.
    EXPECT_TRUE(glIsBuffer(targets));EXPECT_TRUE(glIsBuffer(corners));
    // Test-only readback: exact radial bits must survive even when a low word
    // would be a NaN if transported through a numeric float buffer.
    std::vector<std::uint32_t> packed(18*topology.samples.size());
    glBindBuffer(GL_COPY_READ_BUFFER,targets);
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,packed.size()*4,packed.data());glBindBuffer(GL_COPY_READ_BUFFER,0);
    std::size_t nanWords=0;
    for(std::size_t i=0;i<topology.samples.size();++i) for(int j=0;j<3;++j) {
        const auto lo=packed[9*i+2*j],hi=packed[9*i+2*j+1];
        const auto bits=std::uint64_t(lo)|(std::uint64_t(hi)<<32);
        ASSERT_EQ(bits,std::bit_cast<std::uint64_t>(topology.samples[i].radial[j]));
        nanWords+=(lo&0x7f800000u)==0x7f800000u && (lo&0x007fffffu)!=0;
    }
    EXPECT_GT(nanWords,0u);
    for(std::size_t corner=0;corner<topology.indices.size();++corner) {
        const auto offset=9*topology.samples.size()+9*topology.indices[corner]+6;
        for(int j=0;j<3;++j) EXPECT_EQ(packed[offset+j],std::bit_cast<std::uint32_t>(initial[9*corner+j]));
    }
    struct Bindings {
        GLuint buffer=0;
        ~Bindings() {
            for(int i=0;i<3;++i) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,0);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);glUseProgram(0);
            if(buffer) glDeleteBuffers(1,&buffer);
        }
    } bindings;
    GLint alignment=0;glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,&alignment);
    glGenBuffers(1,&bindings.buffer);glBindBuffer(GL_SHADER_STORAGE_BUFFER,bindings.buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,alignment+64,nullptr,GL_STATIC_DRAW);
    for(int i=0;i<3;++i) glBindBufferRange(GL_SHADER_STORAGE_BUFFER,i,bindings.buffer,alignment,64);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);glUseProgram(compute.program());
    const auto readMesh=[&] {
        GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
        std::vector<float> result(initial.size());glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        glBindBuffer(GL_COPY_READ_BUFFER,mesh.vbo);glGetBufferSubData(GL_COPY_READ_BUFFER,0,result.size()*4,result.data());
        glBindBuffer(GL_COPY_READ_BUFFER,previous);return result;
    };
    const auto east=glm::normalize(glm::cross(eye,glm::dvec3(0,0,1)));
    const auto north=glm::normalize(glm::cross(eye,east));
    const GroundQuery missing=[](const auto&) -> GroundContact {throw std::logic_error("Missing deformed contact plane");};
    double maximumPositionError=0,maximumContactError=0;
    for(double offset:{0.,.05,.15,.30,.15,0.}) {
        const TerrainMorphFootprint footprint{glm::normalize(eye+east*(offset/1000)),1000,.5,.5};
        const auto previousPhase=mesh.morphState->revision();
        compute.deform(mesh,footprint);
        EXPECT_EQ(mesh.morphState->revision(),previousPhase+1);
        compute.deform(mesh,footprint);EXPECT_EQ(mesh.morphState->revision(),previousPhase+1);
        GLint program=0,storage=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);
        glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&storage);
        EXPECT_EQ(program,compute.program());EXPECT_EQ(storage,0);
        for(int i=0;i<3;++i) {
            GLint buffer=0;GLint64 start=0,size=0;
            glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,i,&buffer);
            glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,i,&start);
            glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,i,&size);
            EXPECT_EQ(buffer,bindings.buffer);EXPECT_EQ(start,alignment);EXPECT_EQ(size,64);
        }
        auto reference=topology;
        for(std::size_t i=0;i<reference.samples.size();++i) {
            const auto& r=reference.samples[i].radial;
            const double distance=glm::length(glm::dvec3(r[0],r[1],r[2])-footprint.eyeRadial)*1000;
            const double blend=std::clamp((distance-footprint.radiusMeters)/footprint.transitionMeters,0.,1.);
            reference.morph[i].blend[0]=blend*blend*(3-2*blend);
        }
        reference.canonicalize(surface.field().fingerprint());
        const auto cpu=surface.evaluateTopology(reference);const auto actual=readMesh();
        ASSERT_EQ(actual.size(),cpu.vertices.size());
        std::vector<std::size_t> first(topology.samples.size(),std::numeric_limits<std::size_t>::max());
        double maximum=0;
        for(std::size_t corner=0;corner<topology.indices.size();++corner) {
            const auto p=9*corner;
            const double error=glm::length(glm::dvec3(actual[p]-cpu.vertices[p],actual[p+1]-cpu.vertices[p+1],
                actual[p+2]-cpu.vertices[p+2]))*1000;
            ASSERT_TRUE(std::isfinite(error));maximum=std::max(maximum,error);
            for(int j=3;j<9;++j) ASSERT_EQ(actual[p+j],initial[p+j]);
            auto& original=first[topology.indices[corner]];
            if(original==std::numeric_limits<std::size_t>::max()) original=p;
            EXPECT_EQ(std::memcmp(actual.data()+p,actual.data()+original,12),0);
        }
        EXPECT_LE(maximum,.002); // Preserve the established CPU/GPU position tolerance.
        maximumPositionError=std::max(maximumPositionError,maximum);
        SurfaceContact rendered;rendered.bind(actual,outputIndices,1,1000);
        for(double radius:{.35,.55,.75,.95,1.1}) for(int i=0;i<9;++i) {
            const double angle=(i+.317)*2*std::acos(-1.)/9;
            const auto ray=glm::normalize(footprint.eyeRadial+(east*std::cos(angle)+north*std::sin(angle))*(radius/1000));
            const auto expected=rendered.sample(ray,missing),contact=sharedContact.sample(ray,missing);
            const auto error=glm::length(contact.position-expected.position);
            EXPECT_LE(error,.002);EXPECT_LE(glm::length(contact.normal-expected.normal),1e-5);
            maximumContactError=std::max(maximumContactError,error);
        }
    }
    const TerrainMorphFootprint valid{glm::normalize(eye),1000,.5,.5};
    const auto validRevision=mesh.morphState->revision();
    TerrainComputeLimits disabled;disabled.unavailable="Test unavailable deformation backend";
    TerrainCompute unavailable(disabled);
    auto next=valid;next.radiusMeters=.4;
    EXPECT_THROW(unavailable.deform(mesh,next),std::runtime_error);
    EXPECT_EQ(mesh.morphState->revision(),validRevision);EXPECT_EQ(*mesh.morphState->footprint(),valid);
    auto invalid=valid;invalid.eyeRadial={0,0,0};EXPECT_THROW(compute.deform(mesh,invalid),std::invalid_argument);
    invalid=valid;invalid.transitionMeters=0;EXPECT_THROW(compute.deform(mesh,invalid),std::invalid_argument);
    invalid=valid;invalid.metersPerRadius=2000;EXPECT_THROW(compute.deform(mesh,invalid),std::invalid_argument);
    auto source=mesh.contacts;
    const TerrainSurface wrongField({},lod(),1,1000);
    mesh.contacts=std::make_shared<SparseTerrainContacts>(wrongField.field(),wrongField.buildTopology(1));
    EXPECT_THROW(compute.deform(mesh,valid),std::invalid_argument);mesh.contacts=std::move(source);
    EXPECT_EQ(mesh.morphState->revision(),validRevision);
    Mesh exchanged;mesh.swap(exchanged);EXPECT_EQ(exchanged.morphTargets,targets);EXPECT_EQ(mesh.morphTargets,0u);
    EXPECT_EQ(exchanged.morphState,phaseLifetime.lock());EXPECT_FALSE(mesh.morphState);
    exchanged.swap(mesh);mesh.destroy();EXPECT_FALSE(glIsBuffer(targets));EXPECT_FALSE(glIsBuffer(corners));
    EXPECT_FALSE(mesh.morphState);EXPECT_FALSE(phaseLifetime.expired());
    sharedContact.sample(glm::normalize(eye),missing);sharedContact.clear();EXPECT_TRUE(phaseLifetime.expired());
    EXPECT_THROW(compute.deform(mesh,valid),std::invalid_argument);
    auto replacement=compute.generate(surface.field(),topology,false,TerrainMorphStorage::Retained);
    replacement->waitForCapture();metadata={};static_cast<TerrainBuildStats&>(metadata)=topology;
    mesh.loadComputedTerrain(std::move(metadata),*replacement,false);
    const auto fallbackTargets=mesh.morphTargets,fallbackCorners=mesh.morphCorners;
    mesh.loadTerrain(surface.evaluateTopology(topology));
    EXPECT_FALSE(glIsBuffer(fallbackTargets));EXPECT_FALSE(glIsBuffer(fallbackCorners));
    EXPECT_FALSE(mesh.residentTerrain);EXPECT_EQ(mesh.morphSamples,0u);
    EXPECT_FALSE(mesh.morphState);
    EXPECT_THROW(compute.deform(mesh,valid),std::invalid_argument);
    const TerrainSurface legacy({},lod(),1,1000);
    EXPECT_THROW(compute.generate(legacy.field(),legacy.buildTopology(1),false,TerrainMorphStorage::Retained),std::invalid_argument);
    std::cout << "MORPH_TARGETS samples=" << topology.samples.size() << " corners=" << topology.indices.size()
        << " retained_bytes=" << 72*topology.samples.size()+4*topology.indices.size()
        << " nan_low_words=" << nanWords << " initial_difference=" << initialDifference
        << " maximum_position_error_m=" << maximumPositionError << " maximum_contact_error_m=" << maximumContactError << '\n';
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainCompute, SparseContactGenerationMatchesGpuPlanesWithoutReadingTheCpuMirror) {
    TerrainCompute compute;const TerrainSurface surface({},lod(),1,1000,{},0);
    const auto topology=surface.buildTopologyForEye({1.002,0,0},{0,0,0});
    auto gpu=compute.generate(surface.field(),topology);gpu->waitForCapture();
    const auto vertices=gpu->readVertices();const auto indices=gpu->readIndices();
    auto contacts=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
    SurfaceContact rendered,sparse;rendered.bind(vertices,indices,1,1000);sparse.bind(contacts,1);
    const GroundQuery missing=[](const auto&) -> GroundContact {throw std::logic_error("Missing contact plane");};
    for(const auto& p:{glm::dvec3(1,0,0),glm::dvec3(0,0,1),glm::dvec3(0,0,-1),glm::dvec3(-1,.002,0),glm::dvec3(1,.001,.003)}) {
        const auto expected=rendered.sample(p,missing),actual=sparse.sample(p,missing);
        EXPECT_LE(glm::length(actual.position-expected.position),.002);
        EXPECT_LE(glm::length(actual.normal-expected.normal),1e-5);
    }
    auto geometry=surface.evaluateTopology(topology);std::fill(geometry.vertices.begin(),geometry.vertices.end(),99);
    gpu->contacts=contacts;Mesh mesh;mesh.loadComputedTerrain(std::move(geometry),*gpu);
    sparse.bind(mesh.contacts,mesh.revision);
    EXPECT_LE(glm::length(sparse.sample({1,0,0},missing).position-rendered.sample({1,0,0},missing).position),.002);
    const auto revision=mesh.revision;const auto vbo=mesh.vbo;
    const TerrainSurface other({},lod(),1,2000);
    auto replacement=compute.generate(surface.field(),topology);replacement->waitForCapture();
    replacement->contacts=std::make_shared<SparseTerrainContacts>(other.field(),other.buildTopology(1));
    EXPECT_THROW(mesh.loadComputedTerrain(surface.evaluateTopology(topology),*replacement),std::invalid_argument);
    EXPECT_EQ(mesh.revision,revision);EXPECT_EQ(mesh.vbo,vbo);EXPECT_EQ(mesh.contacts,contacts);
    mesh.loadTerrain(surface.evaluateTopology(topology));EXPECT_FALSE(mesh.contacts);
    mesh.destroy();EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainCompute, StreamingParentProjectionMatchesCpuAndSparseContactsWhileMoving) {
    auto l=lod();l.max_triangle_budget=30000;l.shoreline_edge_m=0;
    l.relief_sinking=true;l.geometric_error_m=.05;l.local_detail_radius_m=.5;
    l.local_edge_m=.05;l.local_transition_m=.5;l.local_detail_streaming=true;
    config::PlanetConfig::SurfaceNoiseFunction n;n.amplitude_m=.01;n.wavelength_m=.25;n.octaves=1;
    const TerrainSurface surface({n},l,1,1000);
    auto limits=TerrainComputeLimits::query();limits.groups=1;TerrainCompute compute(limits);
    for(int step=0;step<2;++step) {
        const auto t=surface.buildTopologyForEye({1.002,step*.00015,0},{});
        // Ensure the independent whole-mesh oracle exercises zero-weight,
        // repeated-parent and actual parent-plane samples, including triplets
        // split across 64-invocation dispatch boundaries.
        std::array<std::size_t,3> kinds{};
        for(const auto& m:t.morph) {
            const bool single=m.parent[0]==m.parent[1] && m.parent[0]==m.parent[2];
            ++kinds[m.blend[0]==0?0:single?1:2];
        }
        for(auto count:kinds) EXPECT_GT(count,0u);
        EXPECT_EQ(t.parentReused,step>0);parity(surface,t,compute,true);
        auto gpu=compute.generate(surface.field(),t);gpu->waitForCapture();
        const auto vertices=gpu->readVertices();const auto indices=gpu->readIndices();
        auto contacts=std::make_shared<SparseTerrainContacts>(surface.field(),t);
        SurfaceContact rendered,sparse;rendered.bind(vertices,indices,1,1000);sparse.bind(contacts,1);
        for(double y:{-.002,-.0003,0.,.0003,.002}) {
            const auto r=glm::normalize(glm::dvec3(1,y,.00013));
            EXPECT_LE(glm::length(rendered.sample(r,{}).position-sparse.sample(r,{}).position),.002);
        }
    }
}
TEST(TerrainCompute, ShortNearBandRebaseKeepsTheCoreUnsunkAndMatchesGpu) {
    auto l=lod();l.max_triangle_budget=30000;l.shoreline_edge_m=0;
    l.relief_sinking=true;l.geometric_error_m=.05;l.local_detail_radius_m=.5;
    l.local_edge_m=.05;l.local_transition_m=.5;l.local_detail_streaming=true;
    l.near_surface_distance_m=2;l.mid_surface_distance_m=20;
    config::PlanetConfig::SurfaceNoiseFunction n;n.amplitude_m=.01;n.wavelength_m=.25;n.octaves=1;
    const TerrainSurface surface({n},l,1,1000);TerrainCompute compute;
    (void)surface.buildTopologyForEye({1.002,0,0},{});
    const glm::dvec3 eye(1.002,.003,0);
    const auto t=surface.buildTopologyForEye(eye,{});
    ASSERT_FALSE(t.parentReused);ASSERT_FALSE(t.localDetailLimited);
    for(const auto& s:t.samples) {
        const glm::dvec3 r(s.radial[0],s.radial[1],s.radial[2]);
        if(glm::length(r-glm::normalize(eye))*1000<l.local_detail_radius_m)
            ASSERT_DOUBLE_EQ(s.sinkMeters,0);
    }
    parity(surface,t,compute,true);
}
TEST(TerrainCompute, VerticalRebasesAndRecordedHighParentsMatchCpuAndContacts) {
    auto l=lod();l.max_triangle_budget=30000;l.shoreline_edge_m=0;
    l.relief_sinking=true;l.geometric_error_m=.05;l.local_detail_radius_m=.5;
    l.local_edge_m=.05;l.local_transition_m=.5;l.local_detail_streaming=true;
    config::PlanetConfig::SurfaceNoiseFunction n;n.amplitude_m=.01;n.wavelength_m=.25;n.octaves=1;
    const TerrainSurface surface({n},l,1,1000);TerrainCompute compute;
    const glm::dvec3 high(1.5,0,0),low(1.002,0,0);
    (void)surface.buildTopologyForEye(high,{});
    for(const bool recorded:{false,true}) {
        if(recorded) surface.prepareReplayParent(high);
        const auto t=surface.buildTopologyForEye(low,{});
        ASSERT_EQ(t.parentReused,recorded);ASSERT_FALSE(t.localDetailLimited);
        parity(surface,t,compute,true);
        auto gpu=compute.generate(surface.field(),t);gpu->waitForCapture();
        const auto vertices=gpu->readVertices();const auto indices=gpu->readIndices();
        auto contacts=std::make_shared<SparseTerrainContacts>(surface.field(),t);
        SurfaceContact rendered,sparse;rendered.bind(vertices,indices,1,1000);sparse.bind(contacts,1);
        for(double y:{-.002,-.0003,0.,.0003,.002}) {
            const auto r=glm::normalize(glm::dvec3(1,y,.00013));
            EXPECT_LE(glm::length(rendered.sample(r,{}).position-sparse.sample(r,{}).position),.002);
        }
        if(!recorded) {
            const auto ascending=surface.buildTopologyForEye(high,{});
            ASSERT_FALSE(ascending.parentReused);parity(surface,ascending,compute,true);
        }
    }
}
int main(int argc,char** argv) {
    ::testing::InitGoogleTest(&argc,argv);if(!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(128,128,"Terrain compute contracts",nullptr,nullptr);if(!window) return 1;
    glfwMakeContextCurrent(window);glewExperimental=GL_TRUE;if(glewInit()!=GLEW_OK) return 1;
    while(glGetError()!=GL_NO_ERROR) {}const int result=RUN_ALL_TESTS();
    glfwDestroyWindow(window);glfwTerminate();return result;
}
