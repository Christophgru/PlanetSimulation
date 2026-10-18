#pragma once
#include "rendering/geometry/terrain/detail/TerrainMorph.h"
#include <charconv>
#include <nlohmann/json.hpp>
#include <string>

namespace rendering {
// A retained original pose still needs fine-anchor grass on replay. Keep that
// case (phase=null) distinct from a body with no retained source at all.
struct TerrainMorphReplay {
    TerrainGenerationKey generation;
    double metersPerRadius=0;
    std::optional<TerrainMorphFootprint> phase;
    void validate() const {
        if(!generation.field || !generation.topology || generation.fieldVersion!=PlanetField::version ||
           generation.topologyVersion!=4 || !std::isfinite(metersPerRadius) || metersPerRadius<=0)
            throw std::invalid_argument("Invalid terrain morph replay identity or scale");
        if(phase) {
            validateTerrainMorphFootprint(*phase);
            if(phase->metersPerRadius!=metersPerRadius)
                throw std::invalid_argument("Terrain morph replay phase scale mismatch");
        }
    }
    // Immutable identity permits an explicit CPU/compute backend override.
    bool matches(const TerrainGenerationKey& key,double scale) const {
        auto expected=generation;expected.backend=key.backend;
        return key==expected && scale==metersPerRadius;
    }
    nlohmann::json json() const {
        validate();
        nlohmann::json result{{"version",1},{"field",std::to_string(generation.field)},
            {"topology",std::to_string(generation.topology)},{"field_version",generation.fieldVersion},
            {"topology_version",generation.topologyVersion},{"meters_per_radius",metersPerRadius},{"footprint",nullptr}};
        if(phase) result["footprint"]={{"eye_radial",{phase->eyeRadial.x,phase->eyeRadial.y,phase->eyeRadial.z}},
            {"radius_m",phase->radiusMeters},{"transition_m",phase->transitionMeters}};
        return result;
    }
    static TerrainMorphReplay fromJson(const nlohmann::json& j) {
        try {
            if(!j.is_object() || !version(j.at("version"),1) ||
               !version(j.at("field_version"),PlanetField::version) || !version(j.at("topology_version"),4))
                throw std::invalid_argument("Unsupported terrain morph replay versions");
            TerrainMorphReplay result;
            result.generation.field=hash(j.at("field"));result.generation.topology=hash(j.at("topology"));
            result.generation.topologyVersion=4;result.metersPerRadius=number(j.at("meters_per_radius"));
            const auto& f=j.at("footprint");
            if(!f.is_null()) {
                if(!f.is_object() || !f.at("eye_radial").is_array() || f.at("eye_radial").size()!=3)
                    throw std::invalid_argument("Terrain morph replay requires a radial three-vector");
                const auto& eye=f.at("eye_radial");
                result.phase=TerrainMorphFootprint{{number(eye[0]),number(eye[1]),number(eye[2])},result.metersPerRadius,
                    number(f.at("radius_m")),number(f.at("transition_m"))};
            }
            result.validate();return result;
        } catch(const nlohmann::json::exception& e) {
            throw std::invalid_argument(std::string("Invalid terrain morph replay: ")+e.what());
        }
    }
private:
    static bool version(const nlohmann::json& j,std::uint32_t expected) {return j.is_number_integer() && j==expected;}
    static double number(const nlohmann::json& j) {
        if(!j.is_number() || !std::isfinite(j.get<double>()))
            throw std::invalid_argument("Terrain morph replay requires finite numbers");
        return j.get<double>();
    }
    static std::uint64_t hash(const nlohmann::json& j) {
        if(!j.is_string()) throw std::invalid_argument("Terrain morph replay requires decimal fingerprints");
        const auto& s=j.get_ref<const std::string&>();std::uint64_t value=0;
        const auto [end,error]=std::from_chars(s.data(),s.data()+s.size(),value);
        if(error!=std::errc{} || end!=s.data()+s.size() || !value || std::to_string(value)!=s)
            throw std::invalid_argument("Invalid terrain morph replay fingerprint");
        return value;
    }
};
}
