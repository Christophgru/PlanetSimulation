#pragma once
#include <glm/glm.hpp>
#include <array>
#include <functional>
#include <vector>
#include <cstdint>
namespace rendering {
struct ExhaustParticle {
    glm::dvec3 position{0},velocity{0}; // Inertial world metres, m/s.
    double age=0,lifetime=1.4;
    std::uint64_t id=0;
    double radius() const { return .035+.10*age/lifetime; }
    double opacity() const;
};
struct ExhaustState {
    std::vector<ExhaustParticle> particles;
    double emissionPhase=0;
    std::uint64_t nextId=0;
};
struct ExhaustEmitter {
    std::array<glm::dvec3,2> nozzles{};
    glm::dvec3 velocity{0},up{0,1,0};
    bool firing=false;
};
struct ExhaustAir { glm::dvec3 velocity{0},gravity{0}; double density=0; };
class ExhaustParticles {
public:
    static constexpr std::size_t capacity=64;
    static constexpr double interval=.025;
    const ExhaustState& state() const { return state_; }
    void restore(const ExhaustState&);
    void clear() { state_={}; }
    void update(double elapsed,const ExhaustEmitter& before,const ExhaustEmitter& after,
                const std::function<ExhaustAir(const glm::dvec3&,double)>& air);
private:
    ExhaustState state_;
};
}
