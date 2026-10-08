#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>
#include <glm/glm.hpp>
#include "config/ScenarioConfig.h"
#include "rendering/geometry/terrain/TerrainSurfacePolicy.h"

namespace rendering {
class TerrainQueryCache;
struct alignas(16) PlanetNoiseParameters {
    double amplitudeMeters=0, frequency=0, persistence=0, lacunarity=0;
    std::uint32_t type=0, octaves=0, seed=0, reserved=0;
    std::array<double,2> padding{};
};
// Explicit std430-compatible layout; no config strings or implicit vec3 padding.
struct alignas(32) PlanetFieldParameters {
    std::array<double,4> scale{}; // radiusWorld, metres/unit, relief bound, sea metres
    std::array<double,4> landscapeShape{};
    std::array<double,4> landscapeMask{};
    std::array<double,4> material{};
    std::array<double,4> gradient{}; // sine, cosine, sample distance metres, reserved
    std::array<std::uint32_t,4> flags{}; // version, noise count, landscape, water
    std::array<std::uint32_t,4> seeds{}; // landscape seed, reserved
    std::array<PlanetNoiseParameters,8> noise{};
};
static_assert(std::is_standard_layout_v<PlanetFieldParameters>);
static_assert(sizeof(PlanetNoiseParameters)==64);
static_assert(sizeof(PlanetFieldParameters)==704);
static_assert(offsetof(PlanetFieldParameters,flags)==160);
static_assert(offsetof(PlanetFieldParameters,noise)==192);
struct PlanetFieldSample { glm::dvec3 position, normal, color; };
struct PlanetFieldGradient { double slope; glm::dvec3 normal; };

// Immutable, camera-independent oracle shared by topology and CPU consumers.
class PlanetField {
public:
    static constexpr std::uint32_t version=1;
    PlanetField(const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions,
                double radiusWorld,double metersPerWorldUnit,
                const config::PlanetConfig::TerrainLandscape& landscape={},
                std::optional<double> waterLevelMeters=std::nullopt,
                const config::PlanetConfig::TerrainMaterial& material={});
    double heightAt(const glm::dvec3& radial,const TerrainSurfacePolicy& policy={}) const;
    double omittedReliefMeters(const glm::dvec3& profile) const;
    double regionPlainWeight(const glm::dvec3& radial) const;
    double regionCliffWeight(const glm::dvec3& radial) const;
    PlanetFieldSample sample(const glm::dvec3& radial,double heightWorld,
                            TerrainQueryCache* queries=nullptr,const TerrainSurfacePolicy& policy={}) const;
    const auto& functions() const { return functions_; }
    double radiusWorld() const { return radius_; }
    double metersPerUnit() const { return metersPerUnit_; }
    double maximumReliefMeters() const { return totalAmplitudeMeters_; }
    std::optional<double> waterLevelMeters() const { return waterLevelMeters_; }
    const PlanetFieldParameters& parameters() const { return parameters_; }
    std::uint64_t fingerprint() const { return fingerprint_; }
    static double smoothstep(double low,double high,double value);
    static glm::dvec3 landscapeColorFactors(double heightMeters,double slope,
        double waterLevelMeters,double beachWidthMeters,double maximumHeightMeters,
        const config::PlanetConfig::TerrainMaterial& material={});
private:
    double heightMeters(const glm::dvec3& direction,const glm::dvec3& profile) const;
    static std::uint32_t hash(int x,int y,int z,int seed);
    double valueNoise(const glm::dvec3& point,int seed) const;
    PlanetFieldGradient gradientAt(const glm::dvec3& radial,double heightMeters,
                                  TerrainQueryCache* queries,const TerrainSurfacePolicy& policy) const;
    glm::dvec3 colorAt(double heightWorld,double slope) const;
    std::vector<config::PlanetConfig::SurfaceNoiseFunction> functions_;
    config::PlanetConfig::TerrainLandscape landscape_;
    double radius_,metersPerUnit_,totalAmplitudeMeters_=0;
    std::optional<double> waterLevelMeters_;
    config::PlanetConfig::TerrainMaterial material_;
    PlanetFieldParameters parameters_{};
    std::uint64_t fingerprint_=0;
};
} // namespace rendering
