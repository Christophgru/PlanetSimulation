#pragma once
#include "rendering/Shader.h"
#include "rendering/foliage/GrassStats.h"
#include "rendering/foliage/procedural/GrassPlan.h"
#include "config/FoliageConfig.h"
#include <memory>
class Mesh;
namespace rendering {
struct GrassPass {
    glm::mat4 model{1}, view{1}, projection{1};
    glm::dvec3 mainEyeBody{0};
    float windTime=0;
    bool feedback=false; // Test probes pause feedback during compute dispatch.
};
struct ProceduralGrassStats {
    std::size_t candidates=0, patches=0, vertices=0, triangles=0, batches=0, patchBytes=0;
    double distanceMeters=0;
    std::size_t gpuBytes=0;
};
class ProceduralGrass {
    struct Batch { GLuint vao=0; GLsizei count=0; };
    struct Draw { std::size_t first=0, patches=0; int level=0, segments=1; };
    struct Bound { glm::dvec3 center; double radius=0; };
    struct Patch {
        std::array<Batch,grassCandidateSlots.size()> batches{};
        std::vector<Bound> bounds;
        std::vector<Draw> draws;
        GLuint buffer=0, vertexTexture=0, indexTexture=0;
        mutable GLuint gpuBlades=0, commands=0;
        mutable std::array<GLuint,2> gpuVaos{};
        mutable std::size_t gpuCapacity=0;
        mutable bool computeUsed=false;
        config::FoliageConfig settings;
        glm::vec3 color{1}, landscapeLevels{0};
        glm::vec2 rockRange{0};
        bool landscape=false, water=false;
        double scale=1;
        std::size_t patches=0;
        glm::dvec3 eye{0};
        std::uint64_t revision=0;
        double distanceMeters=0;
        double density=0;
        int seed=0;
        bool ready=false;
    };
    std::vector<Patch> patches_;
    mutable std::unique_ptr<Shader> compute_;
    void upload(Patch& patch,const GrassPlan& plan);
    void updateDraws(Patch& patch,double scale,double nearDistance,const glm::dvec3& eyeBody);
    void attributes(std::size_t first,int level) const;
    void drawComputed(const Patch& patch,const GrassPass& pass) const;
public:
    Shader shader;
    ProceduralGrass();
    ProceduralGrass(const ProceduralGrass&)=delete;
    ProceduralGrass& operator=(const ProceduralGrass&)=delete;
    ~ProceduralGrass();
    void clear();
    GrassPreparationStats prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
        double metersPerWorldUnit,const glm::dvec3& eyeBody);
    ProceduralGrassStats stats(std::size_t index) const;
    void draw(std::size_t index,const GrassPass* pass=nullptr) const;
    bool usesCompute(std::size_t index) const;
    // Diagnostic readback; never used by the interactive render path.
    std::array<std::size_t,2> computedCounts(std::size_t index) const;
};
}
