#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/geometry/terrain/detail/TerrainMorph.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <bit>
#include <iostream>
#include <map>
#include <numeric>

using namespace rendering;
namespace {
struct SurfaceSource:Mesh {
    explicit SurfaceSource(const glm::dvec3& radial) {
        const auto east=glm::normalize(glm::cross(radial,glm::dvec3(0,1,0)));
        const glm::dmat3 rotation(east,glm::cross(radial,east),radial);
        const std::array<glm::dvec3,3> triangle{{{-.01,-.01,1.002},{.01,-.01,1.002},{0,.01,1.002}}};
        std::array<unsigned,3> mapping{2,0,1};std::array<unsigned,54> words{};
        for(unsigned i=0;i<3;++i) {
            const glm::dvec3 fine(glm::vec3(rotation*triangle[i])),parent=fine*(.998/1.002);
            const glm::vec3 original(parent);const auto direction=glm::normalize(fine);const auto sample=mapping[i];
            for(unsigned j=0;j<3;++j) {
                const auto r=std::bit_cast<std::uint64_t>(direction[j]),b=std::bit_cast<std::uint64_t>(parent[j]);
                words[9*sample+2*j]=unsigned(r);words[9*sample+2*j+1]=unsigned(r>>32);
                words[9*sample+6+j]=std::bit_cast<unsigned>(float(fine[j]));
                words[27+9*sample+2*j]=unsigned(b);words[27+9*sample+2*j+1]=unsigned(b>>32);
                words[27+9*sample+6+j]=std::bit_cast<unsigned>(original[j]);
            }
            for(const auto value:{original,glm::vec3(radial),glm::vec3(.2,.8,.2)})
                for(int j=0;j<3;++j) vertices.push_back(value[j]);
        }
        indices={0,1,2};hasVertexColors=true;upload();vertices.clear();indices.clear();
        residentTerrain=true;revision=1;terrainStats.gpuCorners=terrainStats.uniqueSamples=morphSamples=3;
        terrainStats.generation={123,456,PlanetField::version,4,TerrainBackend::Compute};
        morphState=std::make_shared<TerrainMorphState>();
        glGenBuffers(1,&morphTargets);glBindBuffer(GL_ARRAY_BUFFER,morphTargets);
        glBufferData(GL_ARRAY_BUFFER,sizeof(words),words.data(),GL_STATIC_DRAW);
        glGenBuffers(1,&morphCorners);glBindBuffer(GL_ARRAY_BUFFER,morphCorners);
        glBufferData(GL_ARRAY_BUFFER,sizeof(mapping),mapping.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER,0);
    }
    ~SurfaceSource() {destroy();}
};
struct Blade {glm::vec4 root,up,variation;glm::vec3 wind;unsigned index;};
static_assert(sizeof(Blade)==64);
template<class T> void read(GLuint buffer,std::vector<T>& values) {
    GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
    glBindBuffer(GL_COPY_READ_BUFFER,buffer);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,values.size()*sizeof(T),values.data());
    glBindBuffer(GL_COPY_READ_BUFFER,previous);
}
void uniforms(ProceduralGrass& grass,const GrassPass& pass) {
    grass.shader.use();grass.shader.setMat4("model",&pass.model[0][0]);
    grass.shader.setMat4("view",&pass.view[0][0]);grass.shader.setMat4("projection",&pass.projection[0][0]);
    grass.shader.setFloat("uMetersPerRadius",1000);grass.shader.setFloat("uGrassHeight",1);
    grass.shader.setFloat("uGrassWidth",.08);grass.shader.setFloat("uDrawDistance",40);
    grass.shader.setFloat3("uGrassEyeBody",pass.mainEyeBody.x,pass.mainEyeBody.y,pass.mainEyeBody.z);
    grass.shader.setInt("uLandscapeEnabled",0);grass.shader.setInt("uWaterEnabled",1);
    grass.shader.setFloat3("uLandscapeLevels",0,0,0);grass.shader.setFloat3("uPlanetColor",1,1,1);
    grass.shader.setFloat2("uTerrainRockRange",.3,.5);grass.shader.setFloat("uWaterClearance",0);
    grass.shader.setFloat("uRootOffset",0);grass.shader.setFloat("uTime",pass.windTime);
}
void feedbackProgram(ProceduralGrass& grass) {
    const auto source=Shader::source("shaders/foliage/grass.vert");const char* pointer=source.c_str();
    const GLuint vertex=glCreateShader(GL_VERTEX_SHADER);glShaderSource(vertex,1,&pointer,nullptr);glCompileShader(vertex);
    GLint ok=0;glGetShaderiv(vertex,GL_COMPILE_STATUS,&ok);ASSERT_TRUE(ok);
    glAttachShader(grass.shader.id,vertex);const char* outputs[]{"vRoot","vVisible"};
    glTransformFeedbackVaryings(grass.shader.id,2,outputs,GL_INTERLEAVED_ATTRIBS);glLinkProgram(grass.shader.id);
    glDetachShader(grass.shader.id,vertex);glDeleteShader(vertex);
    glGetProgramiv(grass.shader.id,GL_LINK_STATUS,&ok);ASSERT_TRUE(ok);
}
std::vector<glm::vec4> vertexRoots(ProceduralGrass& grass,const GrassPass& pass,bool configuredFallback=false) {
    uniforms(grass,pass);
    struct Buffer {
        GLuint id=0;
        ~Buffer() {glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&id);}
    } buffer;
    glGenBuffers(1,&buffer.id);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer.id);
    std::vector<glm::vec4> roots(3*grass.stats(0).triangles);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,roots.size()*sizeof(roots[0]),nullptr,GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer.id);glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_TRIANGLES);grass.draw(0,configuredFallback?&pass:nullptr);
    glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
    read(buffer.id,roots);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));return roots;
}
std::map<unsigned,Blade> draw(ProceduralGrass& grass,const GrassPass& pass) {
    uniforms(grass,pass);
    glEnable(GL_RASTERIZER_DISCARD);grass.draw(0,&pass);glDisable(GL_RASTERIZER_DISCARD);
    const auto counts=grass.computedCounts(0);const auto capacity=grass.stats(0).candidates;
    GLint buffer=0;glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,2,&buffer);
    std::vector<Blade> blades(2*capacity);read(buffer,blades);
    std::map<unsigned,Blade> result;
    for(unsigned queue=0;queue<2;++queue) for(unsigned i=0;i<counts[queue];++i) {
        const auto& blade=blades[queue*capacity+i];EXPECT_TRUE(result.emplace(blade.index,blade).second);
    }
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));return result;
}
}

TEST(GrassCells, MovingSurfaceProjectsFixedRootsAndRefreshesPausedAnimationAcrossViewsAndDeltaCommit) {
    config::ScenarioConfig scene(config::Config::load("configs/scenarios/solar_system.json"));auto p=scene.planets[0];
    p.radius=1;p.color={1,1,1};p.surface_noise.clear();p.terrain_landscape.enabled=false;p.water.enabled=true;p.water.level_m=0;
    auto& f=p.foliage;f.enabled=f.compute_placement=true;f.frustum_culling=false;
    f.draw_distance_m=40;f.quad_distance_m=15;f.density_per_m2=2;f.max_blades=100000;
    f.max_candidates_per_triangle=65536;f.root_offset_m=0;f.water_clearance_m=0;
    TerrainCompute terrain;std::size_t shown=0,moved=0,vertexCompared=0;double maximumError=0,maximumPathError=0;
    for(const auto radial:{glm::dvec3(0,0,1),glm::normalize(glm::dvec3(.7,-.2,1))}) {
        SurfaceSource source(radial);ProceduralGrass grass;grass.enableAdaptiveBudget();const auto eye=radial*1.002;
        struct ProgramScope {
            ProgramScope() {glUseProgram(0);}
            ~ProgramScope() {glUseProgram(0);}
        } programScope; // Unbind before destroying this fixture's selected program.
        feedbackProgram(grass);
        auto stage=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,eye,source.revision,
            0,ProceduralGrass::defaultStageBytes,source.morphSource());
        grass.waitForCapture(*stage);grass.reserve(0);grass.commit(0,*stage,source);stage.reset();
        const auto roots=grass.cachedRootsForValidation(0);ASSERT_FALSE(roots.empty());
        ASSERT_TRUE(std::any_of(roots.begin(),roots.end(),[](const auto& r){return r.normalSlot.w!=0;}));
        GrassPass pass;pass.mainEyeBody=eye;pass.windTime=.75f;
        EXPECT_TRUE(draw(grass,pass).empty()); // Initial parent is submerged, potential roots still exist.
        std::map<unsigned,Blade> previous;
        for(const auto band:{glm::dvec2(1000,1),glm::dvec2(9,15),glm::dvec2(5,15),glm::dvec2(0,.1),glm::dvec2(1000,1)}) {
            terrain.deform(source,{radial,1000,band.x,band.y});
            const auto priorFrame=grass.stats(0).animationFrame;
            const auto visible=draw(grass,pass);ASSERT_EQ(grass.stats(0).animationFrame,priorFrame+1);
            if(band.x==1000) ASSERT_FALSE(visible.empty());
            if(band.x==0) EXPECT_TRUE(visible.empty());
            std::vector<float> vertices(27);read(source.vbo,vertices);
            const glm::dvec3 a(vertices[0],vertices[1],vertices[2]),b(vertices[9],vertices[10],vertices[11]),c(vertices[18],vertices[19],vertices[20]);
            const auto n=glm::normalize(glm::cross(b-a,c-a));const double d=glm::dot(n,a);
            const auto cells=grass.cachedCellsForValidation(0);ASSERT_EQ(cells.size(),1u);
            EXPECT_LE(cells[0].metric.x,cells[0].span.y);
            GLint boundBuffer=0;glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,4,&boundBuffer);
            std::vector<glm::vec4> bounds(1);read(boundBuffer,bounds);
            const auto animation=grass.cachedAnimationForValidation(0);
            for(const auto& [id,blade]:visible) {
                ASSERT_LT(id,roots.size());const auto up=glm::dvec3(roots[id].upRank);
                const auto expected=up*(d/glm::dot(n,up));const auto position=glm::dvec3(blade.root);
                const double error=glm::length(position-expected)*1000;maximumError=std::max(maximumError,error);
                ASSERT_LE(error,.001);EXPECT_GE((glm::length(position)-1)*1000,-.0002);
                EXPECT_LE(glm::length(position-glm::dvec3(bounds[0])),bounds[0].w);
                EXPECT_EQ(glm::vec3(blade.up),glm::vec3(roots[id].upRank));EXPECT_EQ(blade.variation,roots[id].variation);
                ASSERT_EQ(animation[id].frame,grass.stats(0).animationFrame);
                if(const auto old=previous.find(id);old!=previous.end()) moved+=old->second.root!=blade.root;
                ++shown;
            }
            auto reflection=pass;reflection.view[0][0]=-1;
            const auto frame=grass.stats(0).animationFrame;
            const auto repeat=draw(grass,reflection);
            EXPECT_EQ(grass.stats(0).animationFrame,frame);ASSERT_EQ(repeat.size(),visible.size());
            const auto reused=grass.cachedAnimationForValidation(0);
            for(const auto& [id,blade]:repeat) {
                EXPECT_EQ(blade.root,visible.at(id).root);EXPECT_EQ(reused[id].frame,animation[id].frame);
                EXPECT_EQ(reused[id].wind,animation[id].wind);EXPECT_EQ(reused[id].trail,animation[id].trail);
            }
            const auto vertex=vertexRoots(grass,pass);ASSERT_EQ(vertex.size(),roots.size()*36);
            std::size_t vertexVisible=0;
            for(unsigned id=0;id<roots.size();++id) {
                const auto& point=vertex[id*36];const bool visibleVertex=point.w>0;
                ASSERT_EQ(visibleVertex,visible.contains(id)) << id;
                if(!visibleVertex) continue;
                const auto error=glm::length(glm::dvec3(point)-glm::dvec3(visible.at(id).root))*1000;
                maximumPathError=std::max(maximumPathError,error);ASSERT_LE(error,.001);
                for(unsigned v=1;v<36;++v) EXPECT_EQ(vertex[id*36+v],point);
                ++vertexVisible;++vertexCompared;
            }
            EXPECT_EQ(vertexVisible,visible.size());
            previous=visible;
        }
        stage=grass.submitResident(source.vbo,source.ebo,source.terrainStats,p,1000,eye,source.revision,
            0,ProceduralGrass::defaultStageBytes,source.morphSource());
        terrain.deform(source,{radial,1000,9,15}); // Phase advances while immutable delta staging is pending.
        grass.waitForCapture(*stage);EXPECT_GT(stage->stats().cellUpdates[4],0u);
        grass.commit(0,*stage,source);stage.reset();ASSERT_FALSE(draw(grass,pass).empty());
        const auto after=grass.cachedRootsForValidation(0);ASSERT_EQ(after.size(),roots.size());
        for(std::size_t i=0;i<roots.size();++i) {
            EXPECT_EQ(after[i].rootExpected,roots[i].rootExpected);EXPECT_EQ(after[i].upRank,roots[i].upRank);
            EXPECT_EQ(after[i].normalSlot,roots[i].normalSlot);EXPECT_EQ(after[i].variation,roots[i].variation);
        }
        p.foliage.compute_placement=false;grass.prepare(0,source,p,1000,eye);
        const auto configured=vertexRoots(grass,pass,true);
        EXPECT_FALSE(grass.usesCompute(0));
        EXPECT_TRUE(std::any_of(configured.begin(),configured.end(),[](const auto& point){return point.w>0;}));
        p.foliage.compute_placement=true;
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_GT(shown,0u);EXPECT_GT(moved,0u);
    std::cout << "GRASS_LIVE_SURFACE visible_queries=" << shown << " moved_common_roots=" << moved
        << " maximum_plane_error_m=" << maximumError << " vertex_compared=" << vertexCompared
        << " maximum_path_error_m=" << maximumPathError << " immutable_roots=1 paused_animation_refresh=1\n";
}
