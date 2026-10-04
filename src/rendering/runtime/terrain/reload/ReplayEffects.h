#pragma once
#include "rendering/character/effects/ExhaustParticles.h"
#include "rendering/foliage/trails/GrassTrail.h"
#include <nlohmann/json.hpp>

namespace rendering::characterReplay {
inline glm::dvec3 vector(const nlohmann::json& j) {
    if(!j.is_array() || j.size()!=3) throw std::invalid_argument("Replay effect needs a three-vector");
    return {j.at(0).get<double>(),j.at(1).get<double>(),j.at(2).get<double>()};
}
inline ExhaustState exhaust(const nlohmann::json& e) {
    if(e.at("schema").get<int>()!=1 || !e.at("particles").is_array() || e.at("particles").size()>ExhaustParticles::capacity)
        throw std::invalid_argument("Unsupported exhaust replay");
    ExhaustState state;
    state.emissionPhase=e.at("emission_phase_s").get<double>();state.nextId=e.at("next_id").get<std::uint64_t>();
    for(const auto& q:e.at("particles")) state.particles.push_back({vector(q.at("position_m")),
        vector(q.at("velocity_mps")),q.at("age_s").get<double>(),q.at("lifetime_s").get<double>(),q.at("id").get<std::uint64_t>()});
    return state;
}
inline std::vector<TrailSegment> trail(const nlohmann::json& entries) {
    if(!entries.is_array() || entries.size()>GrassTrail::capacity)
        throw std::invalid_argument("Astronaut grass trail replay exceeds capacity");
    std::vector<TrailSegment> segments;
    for(const auto& entry:entries) {
        if(!entry.is_array() || entry.size()!=2) throw std::invalid_argument("Grass trail replay needs segment pairs");
        segments.push_back({vector(entry.at(0)),vector(entry.at(1))});
    }
    return segments;
}
inline void validateEffects(const nlohmann::json& pose) {
    if(pose.contains("exhaust")) {ExhaustParticles staged;staged.restore(exhaust(pose.at("exhaust")));}
    if(pose.contains("grass_trail")) {GrassTrail staged;staged.restore(trail(pose.at("grass_trail")));}
}
}
