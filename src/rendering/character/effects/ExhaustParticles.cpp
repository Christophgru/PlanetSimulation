#include "rendering/character/effects/ExhaustParticles.h"
#include <algorithm>
#include "rendering/character/flight/FlightNavigation.h"
#include <cmath>
#include <stdexcept>
#include <unordered_set>
namespace rendering {
double ExhaustParticle::opacity() const {
    const auto smooth=[](double x) { x=std::clamp(x,0.0,1.0); return x*x*(3-2*x); };
    return .28*smooth(age/.06)*(1-smooth((age/lifetime-.60)/.40));
}
void ExhaustParticles::restore(const ExhaustState& s) {
    if (s.particles.size()>capacity || !std::isfinite(s.emissionPhase) || s.emissionPhase<0 || s.emissionPhase>=interval)
        throw std::invalid_argument("Invalid exhaust pool/clock");
    std::unordered_set<std::uint64_t> ids;
    for (const auto& p:s.particles)
        if (!std::isfinite(glm::length(p.position)) || !std::isfinite(glm::length(p.velocity)) ||
            !std::isfinite(p.age) || p.age<0 || !std::isfinite(p.lifetime) || p.lifetime<.5 || p.lifetime>2 ||
            p.age>=p.lifetime || p.id>=s.nextId || !ids.insert(p.id).second)
            throw std::invalid_argument("Invalid exhaust particle");
    state_=s;
}
void ExhaustParticles::update(double elapsed,const ExhaustEmitter& before,const ExhaustEmitter& after,
    const std::function<ExhaustAir(const glm::dvec3&,double)>& air) {
    if (!std::isfinite(elapsed) || elapsed<0 || elapsed>100)
        throw std::invalid_argument("Invalid exhaust elapsed time");
    for (const auto* e:{&before,&after}) {
        if (!std::isfinite(glm::length(e->velocity)) || !std::isfinite(glm::length(e->up)) ||
            std::abs(glm::length(e->up)-1)>1e-6)
            throw std::invalid_argument("Invalid exhaust emitter");
        for (const auto& p:e->nozzles) if (!std::isfinite(glm::length(p)))
            throw std::invalid_argument("Invalid exhaust nozzle");
    }
    if (elapsed==0) return;
    const int steps=std::max(1,int(std::ceil(elapsed/.025)));
    const double dt=elapsed/steps;
    for (int step=1;step<=steps;++step) {
        const double time=step*dt, fraction=time/elapsed;
        for (auto& p:state_.particles) {
            const auto field=air(p.position,time-elapsed);
            const auto old=p.velocity;
            // Small entrained droplets quickly relax toward the gas flow;
            // in vacuum they retain ballistic velocity and world gravity.
            const double drag=6*std::max(0.0,field.density)/1.225;
            p.velocity=field.velocity+(p.velocity+field.gravity*dt-field.velocity)*std::exp(-drag*dt);
            p.position+=(old+p.velocity)*(.5*dt); p.age+=dt;
        }
        std::erase_if(state_.particles,[](const auto& p) { return p.age>=p.lifetime; });
        if (after.firing) {
            state_.emissionPhase+=dt;
            if (state_.emissionPhase+1e-12>=interval) {
                state_.emissionPhase=std::max(0.0,state_.emissionPhase-interval);
                if (state_.particles.size()<capacity) {
                    const auto id=state_.nextId++;
                    const auto up=FlightNavigation::transport(before.up,after.up,fraction)*before.up;
                    const auto right=glm::normalize(glm::cross(up,std::abs(up.x)<.9 ? glm::dvec3(1,0,0) : glm::dvec3(0,1,0)));
                    const auto jitter=right*(.25*std::sin(double(id%1024)*2.399963));
                    state_.particles.push_back({glm::mix(before.nozzles[id%2],after.nozzles[id%2],fraction),
                        glm::mix(before.velocity,after.velocity,fraction)-up*6.0+jitter,0,1.4,id});
                }
            }
        } else state_.emissionPhase=0; // Restart without accumulating a burst.
    }
}
}
