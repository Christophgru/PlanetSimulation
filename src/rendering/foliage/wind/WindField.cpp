#include "rendering/foliage/wind/WindField.h"
#include "rendering/foliage/GrassWind.h"
#include <cstdint>
namespace rendering {
float WindField::perlin(glm::vec3 p,int seed) {
    p-=glm::floor(p/256.f)*256.f;
    auto cell=glm::ivec3(glm::floor(p)); auto f=p-glm::floor(p);
    auto t=f*f*f*(f*(f*6.f-15.f)+10.f);
    const auto gradient=[&](glm::ivec3 c,glm::vec3 offset) {
        std::uint32_t h=std::uint32_t(c.x&255)*1597334677u ^ std::uint32_t(c.y&255)*3812015801u ^
            std::uint32_t(c.z&255)*2798796415u ^ std::uint32_t(seed);
        h^=h>>16; h*=0x7feb352du; h^=h>>15;
        const glm::vec3 gradients[]={{1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
            {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},{0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1}};
        return glm::dot(gradients[h%12],offset);
    };
    return glm::mix(glm::mix(glm::mix(gradient(cell,f),gradient(cell+glm::ivec3(1,0,0),f-glm::vec3(1,0,0)),t.x),
        glm::mix(gradient(cell+glm::ivec3(0,1,0),f-glm::vec3(0,1,0)),gradient(cell+glm::ivec3(1,1,0),f-glm::vec3(1,1,0)),t.x),t.y),
        glm::mix(glm::mix(gradient(cell+glm::ivec3(0,0,1),f-glm::vec3(0,0,1)),gradient(cell+glm::ivec3(1,0,1),f-glm::vec3(1,0,1)),t.x),
        glm::mix(gradient(cell+glm::ivec3(0,1,1),f-glm::vec3(0,1,1)),gradient(cell+glm::ivec3(1,1,1),f-glm::vec3(1,1,1)),t.x),t.y),t.z);
}
glm::vec3 WindField::samples(const config::FoliageConfig& f,glm::dvec3 position,double seconds) {
    const glm::vec3 p(position); const float t=grassWindTime(seconds*f.wind_noise.speed_multiplier);
    return {glm::clamp(.5f+.5f*perlin(p*float(f.wind_noise.gust_frequency)+t*glm::vec3(.25,.125,0),f.wind_noise.seed),0.f,1.f),
        perlin(p*float(f.wind_noise.direction_frequency)+glm::vec3(19.3,7.1,43.7)+t*glm::vec3(.03125,0,0),f.wind_noise.seed),
        perlin(p*float(f.wind_noise.flutter_frequency)+glm::vec3(5.2,31.8,11.4)+t*glm::vec3(0,.5,0),f.wind_noise.seed)};
}
glm::dvec3 WindField::velocity(const config::FoliageConfig& f,glm::dvec3 p,double seconds) {
    if (f.wind_strength==0 || glm::length(p)<1e-9) return glm::dvec3(0);
    const auto s=samples(f,p,seconds),up=glm::vec3(glm::normalize(p));
    const auto right=glm::normalize(glm::cross(std::abs(up.z)<.9f ? glm::vec3(0,0,1) : glm::vec3(0,1,0),up));
    const auto forward=glm::cross(right,up);
    const float gust=.25f+.75f*s.x;
    return glm::dvec3((-std::sin(s.y)*right+std::cos(s.y)*forward)*float(6*f.wind_strength)*gust*gust+
        right*float(.6*f.wind_strength)*s.z);
}
}
