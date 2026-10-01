#pragma once
#include "rendering/Shader.h"
#include "rendering/foliage/GrassStats.h"
#include "rendering/foliage/horizon/HorizonGrassPlan.h"
class Mesh;
namespace rendering {
struct HorizonGrassStats {
    std::size_t candidates=0, patches=0, vertices=0, triangles=0, batches=0, patchBytes=0;
    double distanceMeters=0;
};
class HorizonGrass {
    struct Batch { GLuint vao=0; GLsizei count=0; };
    struct Patch {
        std::array<Batch,8> batches{};
        GLuint buffer=0;
        std::size_t patches=0;
        glm::dvec3 eye{0};
        std::uint64_t revision=0;
        double distanceMeters=0;
        bool ready=false;
    };
    std::vector<Patch> patches_;
    static void upload(Patch& patch,const HorizonGrassPlan& plan);
public:
    Shader shader{"shaders/foliage/horizon.vert","shaders/foliage/grass.frag",
        "shaders/terrain/terrain_shadow.glsl","shaders/atmosphere/atmosphere.glsl"};
    HorizonGrass()=default;
    HorizonGrass(const HorizonGrass&)=delete;
    HorizonGrass& operator=(const HorizonGrass&)=delete;
    ~HorizonGrass();
    void clear();
    GrassPreparationStats prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
        double metersPerWorldUnit,const glm::dvec3& eyeBody);
    HorizonGrassStats stats(std::size_t index) const;
    void draw(std::size_t index) const;
};
}
